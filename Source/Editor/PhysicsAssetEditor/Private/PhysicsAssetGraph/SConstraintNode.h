// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SPhysicsAssetGraphNode.h"

class SConstraintNode : public SPhysicsAssetGraphNode
{
public:
	SLATE_BEGIN_ARGS(SConstraintNode){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, class UPhysicsAssetGraphNode_Constraint* InNode);
};
