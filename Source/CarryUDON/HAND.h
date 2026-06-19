#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "HAND.generated.h"

class UPostProcessComponent;
class UMaterialInstanceDynamic;

UCLASS()
class CARRYUDON_API AHAND : public APawn
{
    GENERATED_BODY()

public:
    AHAND();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USceneComponent* SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UStaticMeshComponent* HandMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UPhysicsHandleComponent* PhysicsHandle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UPostProcessComponent* PostProcessComponent;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float HandFixedDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float CubeTargetDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float InterpSpeed = 25.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
    int32 CurrentScore = 0;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    UMaterialInterface* CurryScreenMaterial;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    float SplashThreshold = 15000.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    float FadeSpeed = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Rules")
    float GameTimeLimit = 30.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    float CurrentTimeRemaining;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    bool bIsGameOver;

    void Grab();
    void Release();
    void MoveUp(float Value);

private:
    bool bIsGrabbing = false;
    float LastCubeDistance = 500.0f;
    float CurrentCurryAlpha = 0.0f;

    UPROPERTY()
    UMaterialInstanceDynamic* DynamicCurryMaterial;

    UPROPERTY()
    class UStaticMeshComponent* StretchingNoodleComp = nullptr;

    FVector NoodleSpawnBaseLocation;
    FVector NoodleOriginalScale;

    UPROPERTY()
    AActor* MyCameraActor;

    UPROPERTY()
    FVector OriginalCameraLocation;

    UPROPERTY()
    AActor* TargetCubeActor;

    // ★末端の滑らかなアニメーション用
    float CurrentVisualPull = 10.0f;

    // ★40回スクロールした後のディレイ（待機）管理用
    float DisappearTimer = 0.0f;
    bool bIsReadyToDisappear = false;
};