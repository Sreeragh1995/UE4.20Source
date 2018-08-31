// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "STimeRangeSlider.h"
#include "Rendering/DrawElements.h"
#include "ITimeSlider.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "STimeRangeSlider"

namespace TimeRangeSliderConstants
{
	const int32 HandleSize = 14;
	const int32 MinimumScrubberWidth = HandleSize * 2;
}

void STimeRangeSlider::Construct( const FArguments& InArgs, TSharedRef<ITimeSliderController> InTimeSliderController)
{
	TimeSliderController = InTimeSliderController;
	LastViewRange = TimeSliderController->GetViewRange();

	ResetState();
	ResetHoveredState();
}

float STimeRangeSlider::ComputeDragDelta(const FPointerEvent& MouseEvent, int32 GeometryWidth) const
{
	float StartTime = 0;
	float EndTime = 0;

	if (TimeSliderController.IsValid())
	{
		StartTime = TimeSliderController->GetClampRange().GetLowerBoundValue();
		EndTime = TimeSliderController->GetClampRange().GetUpperBoundValue();
	}
	float DragDistance = (MouseEvent.GetScreenSpacePosition() - MouseDownPosition).X;

	const float PixelToUnits = (EndTime - StartTime) / (GeometryWidth - TimeRangeSliderConstants::HandleSize*2);
	return DragDistance * PixelToUnits;
}

void STimeRangeSlider::ComputeHandleOffsets(float& LeftHandleOffset, float& HandleOffset, float& RightHandleOffset, int32 GeometryWidth) const
{
	float StartTime = 0;
	float InTime = 0;
	float OutTime = 0;
	float EndTime = 0;

	if (TimeSliderController.IsValid())
	{
		StartTime = TimeSliderController->GetClampRange().GetLowerBoundValue();
		InTime = TimeSliderController->GetViewRange().GetLowerBoundValue();
		OutTime = TimeSliderController->GetViewRange().GetUpperBoundValue();
		EndTime = TimeSliderController->GetClampRange().GetUpperBoundValue();
	}

	const float UnitsToPixel = float(GeometryWidth - TimeRangeSliderConstants::HandleSize*2) / (EndTime - StartTime);

	LeftHandleOffset = (InTime - StartTime) * UnitsToPixel;
	HandleOffset = LeftHandleOffset + TimeRangeSliderConstants::HandleSize;
	RightHandleOffset = HandleOffset + (OutTime - InTime) * UnitsToPixel;
	
	float ScrubberWidth = RightHandleOffset-LeftHandleOffset-TimeRangeSliderConstants::HandleSize;
	if (ScrubberWidth < (float)TimeRangeSliderConstants::MinimumScrubberWidth)
	{
		HandleOffset = HandleOffset - ((float)TimeRangeSliderConstants::MinimumScrubberWidth - ScrubberWidth) / 2.f;
		LeftHandleOffset = HandleOffset - TimeRangeSliderConstants::HandleSize;
		RightHandleOffset = HandleOffset + (float)TimeRangeSliderConstants::MinimumScrubberWidth;
	}
}


FVector2D STimeRangeSlider::ComputeDesiredSize(float) const
{
	return FVector2D(4.0f * TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize);
}


int32 STimeRangeSlider::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const int32 BackgroundLayer = LayerId+1;
	const int32 SliderBoxLayer = BackgroundLayer+1;
	const int32 HandleLayer = SliderBoxLayer+1;

	static const FSlateBrush* RangeHandleLeft = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.RangeHandleLeft" ) ); 
	static const FSlateBrush* RangeHandleRight = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.RangeHandleRight" ) ); 
	static const FSlateBrush* RangeHandle = FEditorStyle::GetBrush( TEXT( "Sequencer.Timeline.RangeHandle" ) ); 

	float LeftHandleOffset = 0.f;
	float HandleOffset = 0.f;
	float RightHandleOffset = 0.f;
	ComputeHandleOffsets(LeftHandleOffset, HandleOffset, RightHandleOffset, AllottedGeometry.GetLocalSize().X);

	static const FName SelectionColorName("SelectionColor");
	FLinearColor SelectionColor = FEditorStyle::GetSlateColor(SelectionColorName).GetColor(FWidgetStyle());

	// Draw the handle box
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(FVector2D(HandleOffset, 0.0f), FVector2D(RightHandleOffset-LeftHandleOffset-TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize)),
		RangeHandle,
		ESlateDrawEffect::None,
		(bHandleDragged || bHandleHovered) ? SelectionColor : FLinearColor::Gray);

	// Draw the left handle box
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(FVector2D(LeftHandleOffset, 0.0f), FVector2D(TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize)),
		RangeHandleLeft,
		ESlateDrawEffect::None,
		(bLeftHandleDragged || bLeftHandleHovered) ? SelectionColor : FLinearColor::Gray);

	// Draw the right handle box
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId, 
		AllottedGeometry.ToPaintGeometry(FVector2D(RightHandleOffset, 0.0f), FVector2D(TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize)),
		RangeHandleRight,
		ESlateDrawEffect::None,
		(bRightHandleDragged || bRightHandleHovered) ? SelectionColor : FLinearColor::Gray);

	SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ));

	return LayerId;
}

FReply STimeRangeSlider::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	MouseDownPosition = MouseEvent.GetScreenSpacePosition();
	if (TimeSliderController.IsValid())
	{		
		MouseDownViewRange = TimeSliderController->GetViewRange();
	}

	if (bHandleHovered)
	{
		bHandleDragged = true;
		return FReply::Handled().CaptureMouse(AsShared());
	}
	else if (bLeftHandleHovered)
	{
		bLeftHandleDragged = true;
		return FReply::Handled().CaptureMouse(AsShared());
	}
	else if (bRightHandleHovered)
	{
		bRightHandleDragged = true;
		return FReply::Handled().CaptureMouse(AsShared());
	}

	return FReply::Unhandled();
}

FReply STimeRangeSlider::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	ResetState();

	return FReply::Handled().ReleaseMouseCapture();
}

FReply STimeRangeSlider::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (HasMouseCapture())
	{
		float DragDelta = ComputeDragDelta(MouseEvent, MyGeometry.GetLocalSize().X);

		ITimeSliderController* TimeSliderControllerPtr = TimeSliderController.Get();
		if (!TimeSliderControllerPtr)
		{
			return FReply::Handled();
		}

		if (bHandleDragged)
		{
			double NewIn = MouseDownViewRange.GetLowerBoundValue() + DragDelta;
			double NewOut = MouseDownViewRange.GetUpperBoundValue() + DragDelta;

			TRange<double> ClampRange = TimeSliderControllerPtr->GetClampRange();
			if (NewIn < ClampRange.GetLowerBoundValue())
			{
				NewIn = ClampRange.GetLowerBoundValue();
				NewOut = NewIn + (MouseDownViewRange.GetUpperBoundValue() - MouseDownViewRange.GetLowerBoundValue());
			}
			else if (NewOut > ClampRange.GetUpperBoundValue())
			{
				NewOut = ClampRange.GetUpperBoundValue();
				NewIn = NewOut - (MouseDownViewRange.GetUpperBoundValue() - MouseDownViewRange.GetLowerBoundValue());
			}
			
			TimeSliderControllerPtr->SetViewRange(NewIn, NewOut, EViewRangeInterpolation::Immediate);
		}
		else if (bLeftHandleDragged || bRightHandleDragged)
		{
			double NewIn = 0.0f;
			double NewOut = 0.0f;

			if (bLeftHandleDragged)
			{
				NewIn = MouseDownViewRange.GetLowerBoundValue() + DragDelta;
				NewOut = MouseDownViewRange.GetUpperBoundValue() - DragDelta;
			}
			else
			{
				NewIn = MouseDownViewRange.GetLowerBoundValue() - DragDelta;
				NewOut = MouseDownViewRange.GetUpperBoundValue() + DragDelta;
			}

			// In cases of extreme zoom the drag delta will be greater than the difference between In/Out.
			// This causes zooming to then become pan at extreme levels which is undesirable.
			if (NewIn >= NewOut)
			{
				return FReply::Handled();
			}

			TimeSliderControllerPtr->SetViewRange(NewIn, NewOut, EViewRangeInterpolation::Immediate);
		}

		return FReply::Handled();
	}
	else
	{
		ResetHoveredState();

		float LeftHandleOffset = 0.f;
		float HandleOffset = 0.f;
		float RightHandleOffset = 0.f;
		ComputeHandleOffsets(LeftHandleOffset, HandleOffset, RightHandleOffset, MyGeometry.GetLocalSize().X);
		
		FGeometry LeftHandleRect  = MyGeometry.MakeChild(FVector2D(LeftHandleOffset, 0.f), FVector2D(TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize));
		FGeometry RightHandleRect = MyGeometry.MakeChild(FVector2D(RightHandleOffset, 0.f), FVector2D(TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize));
		FGeometry HandleRect      = MyGeometry.MakeChild(FVector2D(HandleOffset, 0.0f), FVector2D(RightHandleOffset-LeftHandleOffset-TimeRangeSliderConstants::HandleSize, TimeRangeSliderConstants::HandleSize));

		FVector2D LocalMousePosition = MouseEvent.GetScreenSpacePosition();

		if (HandleRect.IsUnderLocation(LocalMousePosition))
		{
			bHandleHovered = true;
		}
		else if (LeftHandleRect.IsUnderLocation(LocalMousePosition))
		{
			bLeftHandleHovered = true;
		}
		else if (RightHandleRect.IsUnderLocation(LocalMousePosition))
		{
			bRightHandleHovered = true;
		}
	}
	return FReply::Unhandled();
}

void STimeRangeSlider::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave(MouseEvent);

	if (!HasMouseCapture())
	{
		ResetHoveredState();
	}
}

FReply STimeRangeSlider::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	ResetState();

	OnMouseMove(MyGeometry, MouseEvent);

	if (bHandleHovered)
	{
		if (TimeSliderController.IsValid())
		{
			if (FMath::IsNearlyEqual(TimeSliderController->GetViewRange().GetLowerBoundValue(), TimeSliderController->GetClampRange().GetLowerBoundValue()) &&
				FMath::IsNearlyEqual(TimeSliderController->GetViewRange().GetUpperBoundValue(), TimeSliderController->GetClampRange().GetUpperBoundValue()))
			{
				if (!LastViewRange.IsEmpty())
				{
					TimeSliderController->SetViewRange(LastViewRange.GetLowerBoundValue(), LastViewRange.GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
				}
			}
			else
			{
				LastViewRange = TimeSliderController->GetViewRange();
				TimeSliderController->SetViewRange(TimeSliderController->GetClampRange().GetLowerBoundValue(), TimeSliderController->GetClampRange().GetUpperBoundValue(), EViewRangeInterpolation::Immediate);
			}
		}

		ResetState();
		return FReply::Handled();
	}
	ResetState();
	return FReply::Unhandled();
}

void STimeRangeSlider::ResetState()
{
	bHandleDragged = false;
	bLeftHandleDragged = false;
	bRightHandleDragged = false;
	ResetHoveredState();
}

void STimeRangeSlider::ResetHoveredState()
{
	bHandleHovered = false;
	bLeftHandleHovered = false;
	bRightHandleHovered = false;
}

#undef LOCTEXT_NAMESPACE
