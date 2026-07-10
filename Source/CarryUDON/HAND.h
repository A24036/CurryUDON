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

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")

    UMaterialInterface* CurryScreenMaterial;

    // ★変更：計算式を変えたので、初期値を 15000 から 200 ぐらいに下げておきます

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")

    float SplashThreshold = 200.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Curry")

    float FadeSpeed = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Game Rules")

    float GameTimeLimit = 30.0f;

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Sound")

    float EatSoundCooldown = 0.3f;

    // ★追加：食べる音をパッケージ化後も残すための変数

    UPROPERTY(EditAnywhere, Category = "Hand Settings|Sound")

    USoundBase* EatSoundBase;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")

    float CurrentTimeRemaining;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Rules")

    bool bIsGameOver;

    // ★追加：ブループリントから「スタート！」の合図を受け取るための変数

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

    UPROPERTY()

    UMaterialInstanceDynamic* DynamicCurryMaterial;

    // ★変更：1本から「配列（複数本）」に変更

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
