// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "sukoa.generated.h"

/**
 * 
 */
UCLASS()
class CARRYUDON_API Usukoa : public UGameInstance
{
	public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Score", meta = (AllowPrivateAccess = "true"))
	int32 score;

};
