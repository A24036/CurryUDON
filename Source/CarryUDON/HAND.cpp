#include "HAND.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Engine.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "sukoa.h"

// ★ 追加：ケーブルコンポーネント用のインクルード
#include "CableComponent.h"

// ==========================================================
// コンストラクタ
// ==========================================================
AHAND::AHAND()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    HandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMesh"));
    HandMesh->SetupAttachment(SceneRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube"));
    if (MeshAsset.Succeeded())
    {
        HandMesh->SetStaticMesh(MeshAsset.Object);
        HandMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 2.0f));
    }

    HandMesh->SetSimulatePhysics(false);
    HandMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));

    PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
    PostProcessComponent->SetupAttachment(RootComponent);
    PostProcessComponent->bUnbound = true;

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CurryMatAsset(TEXT("Material'/Game/seisaku/curry.curry'"));
    if (CurryMatAsset.Succeeded()) { CurryScreenMaterial = CurryMatAsset.Object; }
}

// ==========================================================
// BeginPlay
// ==========================================================
void AHAND::BeginPlay()
{
    Super::BeginPlay();

    UStaticMesh* NewHandMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/seisaku/Model/hand1.hand1")));
    if (NewHandMesh && HandMesh)
    {
        HandMesh->SetStaticMesh(NewHandMesh);
        HandMesh->SetRelativeScale3D(FVector(-2.0f, 2.0f, 2.0f));
        HandMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, -30.0f));
        HandMesh->SetRelativeLocation(FVector(90.0f, -140.0f, -120.0f));
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC) { PC->bShowMouseCursor = true; }

    if (PhysicsHandle)
    {
        PhysicsHandle->LinearStiffness = 25000.0f;
        PhysicsHandle->LinearDamping = 500.0f;
        PhysicsHandle->InterpolationSpeed = 100.0f;
    }

    CurrentScore = 0;
    CurrentTimeRemaining = GameTimeLimit;
    bIsGameOver = false;

    if (CurryScreenMaterial)
    {
        DynamicCurryMaterial = UMaterialInstanceDynamic::Create(CurryScreenMaterial, this);
        PostProcessComponent->AddOrUpdateBlendable(DynamicCurryMaterial, 1.0f);
        DynamicCurryMaterial->SetScalarParameterValue(FName("Opacity"), 0.0f);
    }

    OriginalCameraLocation = FVector::ZeroVector;
    TargetCubeActor = nullptr;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
    for (AActor* Actor : FoundActors)
    {
        if (Actor->GetName().Contains(TEXT("Camera"))) { MyCameraActor = Actor; }
    }

    TArray<AActor*> FoundCubes;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("TargetCube"), FoundCubes);
    if (FoundCubes.Num() > 0) { TargetCubeActor = FoundCubes[0]; }
}

// ==========================================================
// Tick
// ==========================================================
void AHAND::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsGameOver)
    {
        CurrentTimeRemaining -= DeltaTime;
        if (CurrentTimeRemaining <= 0.0f)
        {
            CurrentTimeRemaining = 0.0f;
            bIsGameOver = true;
            Release();

            Usukoa* GI = Cast<Usukoa>(GetGameInstance());
            if (GI) { GI->score = CurrentScore; }
            UGameplayStatics::OpenLevel(this, FName("NewMap"));
        }
    }

    if (GEngine && !bIsGameOver)
    {
        GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::White, FString::Printf(TEXT("TIME: %.1f"), CurrentTimeRemaining));
        GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, FString::Printf(TEXT("SCORE: %d"), CurrentScore));
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector MouseWorldLoc, MouseWorldDir;
        if (PC->DeprojectMousePositionToWorld(MouseWorldLoc, MouseWorldDir))
        {
            HandFixedDistance = 500.0f;
            FVector HandTargetLoc = MouseWorldLoc + (MouseWorldDir * HandFixedDistance);
            FVector CubeTargetLoc = MouseWorldLoc + (MouseWorldDir * CubeTargetDistance);

            if (bIsGrabbing)
            {
                CurrentVisualPull = FMath::FInterpTo(CurrentVisualPull, CubeTargetDistance, DeltaTime, 10.0f);

                float Speed = FMath::Abs(CubeTargetDistance - LastCubeDistance) / DeltaTime;
                if (Speed > SplashThreshold)
                {
                    CurrentCurryAlpha = 1.0f;
                    CurrentScore -= 200;
                    if (CurrentScore < 0) CurrentScore = 0;
                }
                LastCubeDistance = CubeTargetDistance;

                if (StretchingNoodleComp && NoodleActor)
                {
                    FVector TargetLoc = CubeTargetLoc;
                    FVector TailLoc = NoodleSpawnBaseLocation;

                    if (TargetCubeActor)
                    {
                        FVector BowlLoc = NoodleSpawnBaseLocation;
                        FVector MouthLoc = TargetCubeActor->GetActorLocation();
                        FVector DirToMouth = (MouthLoc - BowlLoc).GetSafeNormal();
                        float MaxDist = FVector::Dist(BowlLoc, MouthLoc);

                        if (CurrentVisualPull <= MaxDist)
                        {
                            TargetLoc = BowlLoc + (DirToMouth * CurrentVisualPull);
                        }
                        else
                        {
                            TargetLoc = MouthLoc;
                            float OverPull = CurrentVisualPull - MaxDist;
                            float MaxOverPull = FMath::Max(0.0f, MaxDist - 15.0f);

                            if (OverPull >= MaxOverPull)
                            {
                                TailLoc = MouthLoc - (DirToMouth * 15.0f);
                            }
                            else
                            {
                                TailLoc = BowlLoc + (DirToMouth * OverPull);
                            }
                        }

                        if (bIsReadyToDisappear)
                        {
                            DisappearTimer += DeltaTime;

                            if (DisappearTimer >= 1.0f)
                            {
                                TailLoc = MouthLoc;
                            }
                            if (DisappearTimer >= 1.5f)
                            {
                                Release();
                                bIsReadyToDisappear = false;
                                DisappearTimer = 0.0f;
                                return;
                            }
                        }
                    }

                    if (StretchingNoodleComp && NoodleActor)
                    {
                        FVector VectorToTarget = TargetLoc - TailLoc;
                        float Distance = VectorToTarget.Size();

                        if (Distance > 1.0f)
                        {
                            StretchingNoodleComp->SetVisibility(true);

                            // 始点（どんぶり側）をセット
                            NoodleActor->SetActorLocation(TailLoc);

                            // 終点（口・マウス側）への相対位置をセット
                            StretchingNoodleComp->EndLocation = VectorToTarget;

                            // 麺の長さを距離に合わせて伸ばす
                            StretchingNoodleComp->CableLength = Distance;
                        }
                        else
                        {
                            StretchingNoodleComp->SetVisibility(false);
                        }
                    }
                }
                else if (PhysicsHandle->GrabbedComponent)
                {
                    PhysicsHandle->SetTargetLocation(CubeTargetLoc);
                }
            }
            SetActorLocation(FMath::VInterpTo(GetActorLocation(), HandTargetLoc, DeltaTime, InterpSpeed));
        }
    }

    if (DynamicCurryMaterial && CurrentCurryAlpha > 0.0f)
    {
        float SlowerFadeSpeed = FadeSpeed * 0.5f;
        CurrentCurryAlpha = FMath::Max(0.0f, CurrentCurryAlpha - SlowerFadeSpeed * DeltaTime);
        DynamicCurryMaterial->SetScalarParameterValue(FName("Opacity"), CurrentCurryAlpha);
    }
}

// ==========================================================
// SetupPlayerInputComponent
// ==========================================================
void AHAND::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &AHAND::Grab);
    PlayerInputComponent->BindAction("Grab", IE_Released, this, &AHAND::Release);
    PlayerInputComponent->BindAxis("MoveUp", this, &AHAND::MoveUp);
}

// ==========================================================
// Grab
// ==========================================================
void AHAND::Grab()
{
    if (bIsGameOver) return;

    FVector GrabCheckLocation = GetActorLocation();
    FVector BoxSize(100.0f, 100.0f, 100.0f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    if (GetWorld()->SweepSingleByChannel(Hit, GrabCheckLocation, GrabCheckLocation + FVector(0, 0, 1), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(BoxSize), Params))
    {
        AActor* HitActor = Hit.GetActor();

        if (HitActor && HitActor->ActorHasTag(FName("Spawner")))
        {
            FVector SpawnLocation = HitActor->GetActorLocation();
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // ケーブルをぶら下げるための「空のアクター」を生成
            NoodleActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnLocation, HitActor->GetActorRotation(), SpawnParams);

            if (NoodleActor)
            {
                USceneComponent* RootComp = NewObject<USceneComponent>(NoodleActor);
                NoodleActor->SetRootComponent(RootComp);
                RootComp->RegisterComponent();

                // ケーブルコンポーネントをアクターに追加
                UCableComponent* CableComp = NewObject<UCableComponent>(NoodleActor);
                CableComp->SetupAttachment(RootComp);

                // ====================================================
                // ★修正：RegisterComponent（計算開始）の【前】に設定を書く！
                // ====================================================
                CableComp->CableWidth = 10.0f; // 麺の太さ
                CableComp->NumSegments = 30;   // 分割数を増やす（ここがクラッシュの原因でした）
                CableComp->CableLength = 0.0f; // 初期は長さ0
                CableComp->EndLocation = FVector::ZeroVector; // 終点初期化

                // スポナー（クリックした対象）からマテリアルを引き継ぐ
                UMeshComponent* ParentComp = Cast<UMeshComponent>(HitActor->GetComponentByClass(UMeshComponent::StaticClass()));
                if (ParentComp)
                {
                    int32 MatCount = ParentComp->GetNumMaterials();
                    for (int32 i = 0; i < MatCount; i++) { CableComp->SetMaterial(i, ParentComp->GetMaterial(i)); }
                }

                // ★すべての設定が終わってから、コンポーネントを登録（有効化）する！
                CableComp->RegisterComponent();

                bIsGrabbing = true;
                StretchingNoodleComp = CableComp;
                NoodleSpawnBaseLocation = SpawnLocation;

                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("!!! つかめた !!! (麺:CableComponent)"));

                bIsReadyToDisappear = false;
                DisappearTimer = 0.0f;

                if (MyCameraActor)
                {
                    if (OriginalCameraLocation == FVector::ZeroVector)
                    {
                        OriginalCameraLocation = MyCameraActor->GetActorLocation();
                    }
                    FVector DirToNoodle = (NoodleSpawnBaseLocation - OriginalCameraLocation).GetSafeNormal();
                    FVector ZoomLocation = NoodleSpawnBaseLocation - (DirToNoodle * 200.0f);
                    MyCameraActor->SetActorLocation(ZoomLocation);
                }

                FVector CamLoc = MyCameraActor ? MyCameraActor->GetActorLocation() : GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();

                CubeTargetDistance = FVector::Dist(NoodleSpawnBaseLocation, CamLoc);
                CurrentVisualPull = CubeTargetDistance;
                LastCubeDistance = CubeTargetDistance;
            }
            return;
        }

        UPrimitiveComponent* HitComp = Hit.GetComponent();
        if (HitComp && HitComp->IsSimulatingPhysics())
        {
            FVector CamLoc = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
            CubeTargetDistance = FVector::Dist(HitActor->GetActorLocation(), CamLoc);
            LastCubeDistance = CubeTargetDistance;

            bIsGrabbing = true;
            PhysicsHandle->GrabbedComponent = HitComp;
            PhysicsHandle->GrabComponentAtLocationWithRotation(HitComp, NAME_None, HitComp->GetComponentLocation(), HitComp->GetComponentRotation());

            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("!!! つかめた !!! (物理オブジェクト)"));
        }
    }
}

// ==========================================================
// Release
// ==========================================================
void AHAND::Release()
{
    bIsGrabbing = false;

    if (NoodleActor)
    {
        NoodleActor->Destroy();
        NoodleActor = nullptr;
        StretchingNoodleComp = nullptr;
    }

    if (PhysicsHandle->GrabbedComponent)
    {
        PhysicsHandle->ReleaseComponent();
    }

    if (MyCameraActor && OriginalCameraLocation != FVector::ZeroVector)
    {
        MyCameraActor->SetActorLocation(OriginalCameraLocation);
        OriginalCameraLocation = FVector::ZeroVector;
    }
}

// ==========================================================
// MoveUp
// ==========================================================
void AHAND::MoveUp(float Value)
{
    if (bIsGameOver) return;

    static int32 WheelRotationCount = 0;
    if (!bIsGrabbing)
    {
        WheelRotationCount = 0;
    }

    if (Value != 0.0f && !bIsReadyToDisappear)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(10, 0.5f, FColor::Cyan, TEXT("--- Wheel Input Detected! ---"));

        CubeTargetDistance += Value * 150.0f;
        CubeTargetDistance = FMath::Clamp(CubeTargetDistance, 10.0f, 300000.0f);

        if (bIsGrabbing)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(11, 0.5f, FColor::Green, TEXT("Status: Grabbing & Suctioning!"));

            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);

            USoundBase* EatSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/eat.eat")));
            if (EatSound)
            {
                UGameplayStatics::PlaySound2D(this, EatSound);
                if (GEngine) GEngine->AddOnScreenDebugMessage(12, 0.5f, FColor::Yellow, TEXT("Sound File: FOUND & Playing!"));
            }

            WheelRotationCount++;

            if (WheelRotationCount >= 25)
            {
                bIsReadyToDisappear = true;
                WheelRotationCount = 0;
            }
        }
    }
}