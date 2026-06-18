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
    if (CurryMatAsset.Succeeded())
    {
        CurryScreenMaterial = CurryMatAsset.Object;
    }
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
        if (Actor->GetName().Contains(TEXT("Camera")))
        {
            MyCameraActor = Actor;
            break;
        }
    }

    TArray<AActor*> FoundCubes;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("TargetCube"), FoundCubes);
    if (FoundCubes.Num() > 0)
    {
        TargetCubeActor = FoundCubes[0];
    }
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
            if (GI)
            {
                GI->score = CurrentScore;
            }
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
                float Speed = FMath::Abs(CubeTargetDistance - LastCubeDistance) / DeltaTime;

                if (Speed > SplashThreshold)
                {
                    CurrentCurryAlpha = 1.0f;
                    CurrentScore -= 200;
                    if (CurrentScore < 0) CurrentScore = 0;
                }
                LastCubeDistance = CubeTargetDistance;

                if (StretchingNoodleComp)
                {
                    FVector TipLoc = CubeTargetLoc;  // 先端（口側）
                    FVector TailLoc = NoodleSpawnBaseLocation; // 末端（器側）

                    if (TargetCubeActor)
                    {
                        FVector BowlLoc = NoodleSpawnBaseLocation;
                        FVector MouthLoc = TargetCubeActor->GetActorLocation();
                        FVector DirToMouth = (MouthLoc - BowlLoc).GetSafeNormal();
                        float MaxDist = FVector::Dist(BowlLoc, MouthLoc);

                        if (CubeTargetDistance <= MaxDist)
                        {
                            // 1. まだ口(Cube3)に届いていない：器から口へ向かって伸びる
                            TailLoc = BowlLoc;
                            TipLoc = BowlLoc + (DirToMouth * CubeTargetDistance);
                            DisappearTimer = 0.0f; // 吸い込み中はタイマーをリセット
                        }
                        else
                        {
                            // 2. 口(Cube3)に届いた後：末端が器から離れて上に吸い込まれる
                            TipLoc = MouthLoc;
                            float OverPull = CubeTargetDistance - MaxDist;

                            // ★変更：完全に吸い込み終わる直前で、口元に少し(15cm)だけ麺を残してストップする
                            if (OverPull >= MaxDist - 15.0f)
                            {
                                TailLoc = MouthLoc - (DirToMouth * 15.0f); // 口元に末端を残す

                                // ★変更：ディレイタイマーのカウント開始
                                DisappearTimer += DeltaTime;

                                // 0.6秒間(ディレイ)経過したら長さを0にして消滅させる
                                if (DisappearTimer >= 0.6f)
                                {
                                    TailLoc = MouthLoc;
                                }
                            }
                            else
                            {
                                TailLoc = BowlLoc + (DirToMouth * OverPull);
                                DisappearTimer = 0.0f;
                            }
                        }
                    }

                    FVector VectorToTarget = TipLoc - TailLoc;
                    float Distance = VectorToTarget.Size();

                    // 距離がある場合は麺を描画
                    if (Distance > 1.0f)
                    {
                        StretchingNoodleComp->SetVisibility(true);
                        FVector MidPoint = TailLoc + (VectorToTarget * 0.5f);
                        StretchingNoodleComp->SetWorldLocation(MidPoint);

                        FRotator NewRot = FRotationMatrix::MakeFromZ(VectorToTarget).Rotator();
                        StretchingNoodleComp->SetWorldRotation(NewRot);

                        float MeshHeight = StretchingNoodleComp->GetStaticMesh()->GetBoundingBox().GetSize().Z;
                        if (MeshHeight <= 0.1f) MeshHeight = 100.0f;

                        float StretchScaleZ = Distance / MeshHeight;
                        StretchingNoodleComp->SetWorldScale3D(FVector(NoodleOriginalScale.X, NoodleOriginalScale.Y, StretchScaleZ));
                    }
                    else
                    {
                        // 長さがゼロになったら非表示にする
                        StretchingNoodleComp->SetVisibility(false);

                        // ★追加：ディレイが完了して完全に消えたら、自動的にRelease()を呼び出して次のうどんを食べられるようにする
                        if (DisappearTimer >= 0.6f)
                        {
                            Release();
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

    if (GetWorld()->SweepSingleByChannel(Hit, GrabCheckLocation, GrabCheckLocation + FVector(0, 0, 1), FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeBox(BoxSize), Params))
    {
        AActor* HitActor = Hit.GetActor();

        if (HitActor && HitActor->ActorHasTag(FName("Spawner")))
        {
            FVector SpawnLocation = HitActor->GetActorLocation();

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AStaticMeshActor* NewBlock = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLocation, HitActor->GetActorRotation(), SpawnParams);

            if (NewBlock)
            {
                NewBlock->GetRootComponent()->SetMobility(EComponentMobility::Movable);
                UStaticMeshComponent* ParentComp = Cast<UStaticMeshComponent>(HitActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
                UStaticMeshComponent* NewComp = NewBlock->GetStaticMeshComponent();

                if (ParentComp && NewComp)
                {
                    NewComp->SetStaticMesh(ParentComp->GetStaticMesh());
                    int32 MatCount = ParentComp->GetNumMaterials();
                    for (int32 i = 0; i < MatCount; i++) { NewComp->SetMaterial(i, ParentComp->GetMaterial(i)); }

                    NoodleOriginalScale = ParentComp->GetComponentScale();
                    NewComp->SetWorldScale3D(NoodleOriginalScale);

                    NewComp->SetSimulatePhysics(false);
                    NewComp->SetCollisionProfileName(TEXT("NoCollision"));

                    bIsGrabbing = true;
                    StretchingNoodleComp = NewComp;
                    NoodleSpawnBaseLocation = SpawnLocation;

                    CubeTargetDistance = 10.0f;
                    LastCubeDistance = CubeTargetDistance;

                    // ★追加：掴み直すたびにタイマーをリセット
                    DisappearTimer = 0.0f;
                }
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

    if (StretchingNoodleComp)
    {
        AActor* NoodleActor = StretchingNoodleComp->GetOwner();
        if (NoodleActor) { NoodleActor->Destroy(); }
        StretchingNoodleComp = nullptr;
    }

    if (PhysicsHandle->GrabbedComponent)
    {
        PhysicsHandle->ReleaseComponent();
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

    if (Value != 0.0f)
    {
        CubeTargetDistance += Value * 150.0f;

        if (bIsGrabbing)
        {
            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);

            USoundBase* EatSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/eat.eat")));
            if (EatSound)
            {
                UGameplayStatics::PlaySound2D(this, EatSound);
            }

            WheelRotationCount++;
            if (WheelRotationCount >= 40)
            {
                // ここでのReleaseは一旦残しておきますが、Tick側で自動消滅するためあまり呼ばれなくなります
                Release();
                WheelRotationCount = 0;
            }
        }
    }
}