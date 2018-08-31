// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "HttpThread.h"
#include "IHttpThreadedRequest.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "HttpModule.h"
#include "Http.h"

// FHttpThread

FHttpThread::FHttpThread()
	:	Thread(nullptr)
{
	HttpThreadActiveFrameTimeInSeconds = FHttpModule::Get().GetHttpThreadActiveFrameTimeInSeconds();
	HttpThreadActiveMinimumSleepTimeInSeconds = FHttpModule::Get().GetHttpThreadActiveMinimumSleepTimeInSeconds();
	HttpThreadIdleFrameTimeInSeconds = FHttpModule::Get().GetHttpThreadIdleFrameTimeInSeconds();
	HttpThreadIdleMinimumSleepTimeInSeconds = FHttpModule::Get().GetHttpThreadIdleMinimumSleepTimeInSeconds();

	UE_LOG(LogHttp, Log, TEXT("HTTP thread active frame time %.1f ms. Minimum active sleep time is %.1f ms. HTTP thread idle frame time %.1f ms. Minimum idle sleep time is %.1f ms."), HttpThreadActiveFrameTimeInSeconds * 1000.0, HttpThreadActiveMinimumSleepTimeInSeconds * 1000.0, HttpThreadIdleFrameTimeInSeconds * 1000.0, HttpThreadIdleMinimumSleepTimeInSeconds * 1000.0);
}

FHttpThread::~FHttpThread()
{
	StopThread();
}

void FHttpThread::StartThread()
{
	Thread = FRunnableThread::Create(this, TEXT("HttpManagerThread"), 128 * 1024, TPri_Normal);
}

void FHttpThread::StopThread()
{
	if (Thread != nullptr)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}
}

void FHttpThread::AddRequest(IHttpThreadedRequest* Request)
{
	PendingThreadedRequests.Enqueue(Request);
}

void FHttpThread::CancelRequest(IHttpThreadedRequest* Request)
{
	CancelledThreadedRequests.Enqueue(Request);
}

void FHttpThread::GetCompletedRequests(TArray<IHttpThreadedRequest*>& OutCompletedRequests)
{
	check(IsInGameThread());
	IHttpThreadedRequest* Request = nullptr;
	while (CompletedThreadedRequests.Dequeue(Request))
	{
		OutCompletedRequests.Add(Request);
	}
}

bool FHttpThread::Init()
{
	LastTime = FPlatformTime::Seconds();
	return true;
}

uint32 FHttpThread::Run()
{
	// Arrays declared outside of loop to re-use memory
	TArray<IHttpThreadedRequest*> RequestsToCancel;
	TArray<IHttpThreadedRequest*> RequestsToStart;
	TArray<IHttpThreadedRequest*> RequestsToComplete;
	while (!ExitRequest.GetValue())
	{
		double OuterLoopBegin = FPlatformTime::Seconds();
		double OuterLoopEnd = 0.0;
		bool bKeepProcessing = true;
		while (bKeepProcessing)
		{
			double InnerLoopBegin = FPlatformTime::Seconds();
			
			Process(RequestsToCancel, RequestsToStart, RequestsToComplete);
			
			if (RunningThreadedRequests.Num() == 0)
			{
				bKeepProcessing = false;
			}

			double InnerLoopEnd = FPlatformTime::Seconds();
			if (bKeepProcessing)
			{
				double InnerLoopTime = InnerLoopEnd - InnerLoopBegin;
				double InnerSleep = FMath::Max(HttpThreadActiveFrameTimeInSeconds - InnerLoopTime, HttpThreadActiveMinimumSleepTimeInSeconds);
				FPlatformProcess::SleepNoStats(InnerSleep);
			}
			else
			{
				OuterLoopEnd = InnerLoopEnd;
			}
		}
		double OuterLoopTime = OuterLoopEnd - OuterLoopBegin;
		double OuterSleep = FMath::Max(HttpThreadIdleFrameTimeInSeconds - OuterLoopTime, HttpThreadIdleMinimumSleepTimeInSeconds);
		FPlatformProcess::SleepNoStats(OuterSleep);
	}
	return 0;
}

void FHttpThread::Tick()
{
	TArray<IHttpThreadedRequest*> RequestsToCancel;
	TArray<IHttpThreadedRequest*> RequestsToStart;
	TArray<IHttpThreadedRequest*> RequestsToComplete;
	Process(RequestsToCancel, RequestsToStart, RequestsToComplete);
}

void FHttpThread::HttpThreadTick(float DeltaSeconds)
{
	// empty
}

bool FHttpThread::StartThreadedRequest(IHttpThreadedRequest* Request)
{
	return Request->StartThreadedRequest();
}

void FHttpThread::CompleteThreadedRequest(IHttpThreadedRequest* Request)
{
	// empty
}

void FHttpThread::Process(TArray<IHttpThreadedRequest*>& RequestsToCancel, TArray<IHttpThreadedRequest*>& RequestsToStart, TArray<IHttpThreadedRequest*>& RequestsToComplete)
{
	// cache all cancelled and pending requests
	{
		IHttpThreadedRequest* Request = nullptr;

		RequestsToCancel.Reset();
		while (CancelledThreadedRequests.Dequeue(Request))
		{
			RequestsToCancel.Add(Request);
		}

		RequestsToStart.Reset();
		while (PendingThreadedRequests.Dequeue(Request))
		{
			RequestsToStart.Add(Request);
		}
	}

	// Cancel any pending cancel requests
	for (IHttpThreadedRequest* Request : RequestsToCancel)
	{
		if (RunningThreadedRequests.Remove(Request) > 0)
		{
			RequestsToComplete.Add(Request);
		}
	}

	const double AppTime = FPlatformTime::Seconds();
	const double ElapsedTime = AppTime - LastTime;
	LastTime = AppTime;

	// Tick any running requests
	// as long as they properly finish in HttpThreadTick below they are unaffected by a possibly large ElapsedTime above
	for (IHttpThreadedRequest* Request : RunningThreadedRequests)
	{
		Request->TickThreadedRequest(ElapsedTime);
	}

	// Start any pending requests
	// Tick new requests separately from existing RunningThreadedRequests so they get a chance 
	// to send unaffected by possibly large ElapsedTime above
	for (IHttpThreadedRequest* Request : RequestsToStart)
	{
		if (StartThreadedRequest(Request))
		{
			RunningThreadedRequests.Add(Request);
			Request->TickThreadedRequest(0.0f);
		}
		else
		{
			RequestsToComplete.Add(Request);
		}
	}

	// Every valid request in RunningThreadedRequests gets at least two calls to HttpThreadTick
	// Blocking loads still can affect things if the network stack can't keep its connections alive
	HttpThreadTick(ElapsedTime);

	// Move any completed requests
	for (int32 Index = 0; Index < RunningThreadedRequests.Num(); ++Index)
	{
		IHttpThreadedRequest* Request = RunningThreadedRequests[Index];
		if (Request->IsThreadedRequestComplete())
		{
			RequestsToComplete.Add(Request);
			RunningThreadedRequests.RemoveAtSwap(Index);
			--Index;
		}
	}

	if (RequestsToComplete.Num() > 0)
	{
		for (IHttpThreadedRequest* Request : RequestsToComplete)
		{
			CompleteThreadedRequest(Request);
			CompletedThreadedRequests.Enqueue(Request);
		}
		RequestsToComplete.Reset();
	}
}

void FHttpThread::Stop()
{
	ExitRequest.Set(true);
}
	
void FHttpThread::Exit()
{
	// empty
}
