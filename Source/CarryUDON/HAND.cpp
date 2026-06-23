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

// ケーブルコンポーネント用のインクルード
#include "CableComponent.h"

// オーディオコンポーネント用のインクルード
#include "Components/AudioComponent.h"

// ★追加：ご自身のGameInstanceのヘッダーファイルを読み込む
// （※ファイル名は実際のプロジェクトに合わせて変更してください）
#include "sukoa.h"

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

    BGMAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BGMAudioComponent"));
    BGMAudioComponent->SetupAttachment(RootComponent);

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CurryMatAsset(TEXT("Material'/Game/seisaku/curry.curry'"));
    if (CurryMatAsset.Succeeded()) { CurryScreenMaterial = CurryMatAsset.Object; }
}

// ==========================================================
// BeginPlay
// ==========================================================
void AHAND::BeginPlay()
{
    Super::BeginPlay();

    if (BGMAudioComponent)
    {
        USoundBase* NewBGM = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/onsen-ryokan-24.onsen-ryokan-24")));
        if (NewBGM)
        {
            BGMAudioComponent->SetSound(NewBGM);
            BGMAudioComponent->Play();
        }
    }

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

            // ★追加：レベル遷移の直前にGameInstanceを取得してスコアを渡す
            // （※ UMyGameInstance と SavedScore は実際の名称に変更してください）
            Usukoa* GameInst = Cast<Usukoa>(UGameplayStatics::GetGameInstance(this));
            if (GameInst)
            {
                GameInst->score = CurrentScore;
            }

            UGameplayStatics::OpenLevel(this, FName("NewMap"));
        }
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector MouseWorldLoc, MouseWorldDir;
        if (PC->DeprojectMousePositionToWorld(MouseWorldLoc, MouseWorldDir))
        {
            HandFixedDistance = 500.0f;

            if (!bIsGrabbing)
            {
                CurrentHandZOffset = FMath::FInterpTo(CurrentHandZOffset, 0.0f, DeltaTime, 8.0f);
            }
            else
            {
                CurrentHandZOffset = FMath::FInterpTo(CurrentHandZOffset, 0.0f, DeltaTime, 3.0f);
            }

            FVector HandTargetLoc = MouseWorldLoc + (MouseWorldDir * HandFixedDistance) + FVector(0.0f, 0.0f, CurrentHandZOffset);
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

                if (StretchingNoodleComps.Num() > 0 && NoodleActor)
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

                    if (StretchingNoodleComps.Num() > 0 && NoodleActor)
                    {
                        FVector VectorToTarget = TargetLoc - TailLoc;
                        float Distance = VectorToTarget.Size();

                        NoodleActor->SetActorLocation(TailLoc);

                        // ★変更：3本の麺それぞれに対して長さを更新
                        for (UCableComponent* Cable : StretchingNoodleComps)
                        {
                            if (Cable)
                            {
                                if (Distance > 1.0f)
                                {
                                    Cable->SetVisibility(true);
                                    Cable->EndLocation = VectorToTarget;
                                    Cable->CableLength = Distance;
                                }
                                else
                                {
                                    Cable->SetVisibility(false);
                                }
                            }
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

    bool bHit = GetWorld()->SweepSingleByChannel(Hit, GrabCheckLocation, GrabCheckLocation + FVector(0, 0, 1), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeBox(BoxSize), Params);

    if (bHit)
    {
        AActor* HitActor = Hit.GetActor();

        if (HitActor && HitActor->ActorHasTag(FName("Spawner")))
        {
            FVector SpawnLocation = HitActor->GetActorLocation();
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            NoodleActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), SpawnLocation, HitActor->GetActorRotation(), SpawnParams);

            if (NoodleActor)
            {
                USceneComponent* RootComp = NewObject<USceneComponent>(NoodleActor);
                NoodleActor->SetRootComponent(RootComp);
                RootComp->RegisterComponent();

                StretchingNoodleComps.Empty();

                // ★変更：ここでケーブル（麺）を3本生成して、少しずつ位置をずらす
                for (int32 i = 0; i < 3; i++)
                {
                    UCableComponent* CableComp = NewObject<UCableComponent>(NoodleActor);
                    CableComp->SetupAttachment(RootComp);

                    // 3本が重ならないように根元の位置を少しずらす
                    FVector Offset(0.0f, 0.0f, 0.0f);
                    if (i == 0) Offset = FVector(8.0f, 0.0f, 0.0f);
                    if (i == 1) Offset = FVector(-4.0f, 7.0f, 0.0f);
                    if (i == 2) Offset = FVector(-4.0f, -7.0f, 0.0f);
                    CableComp->SetRelativeLocation(Offset);

                    // 3本まとまるので、1本あたりの太さを少し細めに設定
                    CableComp->CableWidth = 8.0f;
                    CableComp->NumSegments = 30;
                    CableComp->CableLength = 0.0f;
                    CableComp->EndLocation = FVector::ZeroVector;

                    UMeshComponent* ParentComp = Cast<UMeshComponent>(HitActor->GetComponentByClass(UMeshComponent::StaticClass()));
                    if (ParentComp)
                    {
                        int32 MatCount = ParentComp->GetNumMaterials();
                        for (int32 matIdx = 0; matIdx < MatCount; matIdx++)
                        {
                            CableComp->SetMaterial(matIdx, ParentComp->GetMaterial(matIdx));
                        }
                    }

                    CableComp->RegisterComponent();
                    StretchingNoodleComps.Add(CableComp);
                }

                bIsGrabbing = true;
                NoodleSpawnBaseLocation = SpawnLocation;

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
        // ★変更：複数本の麺の情報をリセット
        StretchingNoodleComps.Empty();
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
        CubeTargetDistance += Value * 150.0f;
        CubeTargetDistance = FMath::Clamp(CubeTargetDistance, 10.0f, 300000.0f);

        if (bIsGrabbing)
        {
            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);

            CurrentHandZOffset += FMath::Abs(Value) * 80.0f;
            CurrentHandZOffset = FMath::Clamp(CurrentHandZOffset, 0.0f, 600.0f);

            float CurrentTime = GetWorld()->GetTimeSeconds();

            if (CurrentTime >= LastEatSoundTime + EatSoundCooldown)
            {
                USoundBase* EatSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/eat.eat")));

                if (EatSound)
                {
                    UGameplayStatics::PlaySound2D(this, EatSound);
                    LastEatSoundTime = CurrentTime;
                }
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