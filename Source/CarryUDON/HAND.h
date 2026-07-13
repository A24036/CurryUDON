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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UAudioComponent* BGMAudioComponent;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float HandFixedDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float CubeTargetDistance = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings")
    float InterpSpeed = 25.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Score")
    int32 CurrentScore = 0;

    // ★ パッケージ化で消えないように UPROPERTY で保護されたマテリアル変数
    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    UMaterialInterface* CurryScreenMaterial;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    float CurrySplashSpeedLimit = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")
    float FadeSpeed = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")
    float GameTimeLimit = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Sound")
    float EatSoundCooldown = 0.3f;

    UPROPERTY()
    USoundBase* EatSoundAsset;

    UPROPERTY()
    USoundBase* BGMSoundAsset;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    float CurrentTimeRemaining;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")
    bool bIsGameOver;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Game Rules")
    bool bIsGameStarted;

    void Grab();
    void Release();
    void MoveUp(float Value);

private:
    bool bIsGrabbing = false;
    float LastCubeDistance = 500.0f;
    float CurrentCurryAlpha = 0.0f;

    float CurrentHandZOffset = 0.0f;
    float LastEatSoundTime = 0.0f;

    float LastScrollTime = 0.0f;

    UPROPERTY()
    UMaterialInstanceDynamic* DynamicCurryMaterial;

    UPROPERTY()
    TArray<class UCableComponent*> StretchingNoodleComps;

    UPROPERTY()
    AActor* NoodleActor = nullptr;

    FVector NoodleSpawnBaseLocation;
    FVector NoodleOriginalScale;

    UPROPERTY()
    AActor* MyCameraActor;

    UPROPERTY()
    FVector OriginalCameraLocation;

    UPROPERTY()
    AActor* TargetCubeActor;

    float CurrentVisualPull = 10.0f;
    float DisappearTimer = 0.0f;
    bool bIsReadyToDisappear = false;
};