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

void AHAND::BeginPlay()
{
    Super::BeginPlay();

    UStaticMesh* NewHandMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/seisaku/Model/hand1.hand1")));
    if (NewHandMesh && HandMesh)
    {
        HandMesh->SetStaticMesh(NewHandMesh);
        HandMesh->SetRelativeScale3D(FVector(-1.0f, 1.0f, 1.0f));
        HandMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));

        // ★【修正】箸の先端を手前の箸（麺）に合わせるための位置ズレ（オフセット）★
        // 画像 image_12.png に基づき、箸先を下(-55.0)に下げ、
        // かつカメラ側(-25.0)に引き、左右の位置(20.0)も微調整しました。
        // これで箸先と箸（麺）オブジェクトが重なるはずです！
        HandMesh->SetRelativeLocation(FVector(20.0f, 55.0f, -55.0f));
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
}

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
            UGameplayStatics::OpenLevel(this, FName("NewMap"));
        }
    }

    if (GEngine)
    {
        if (!bIsGameOver)
        {
            GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::White, FString::Printf(TEXT("TIME: %.1f"), CurrentTimeRemaining));
            GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, FString::Printf(TEXT("SCORE: %d"), CurrentScore));
        }
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector MouseWorldLoc, MouseWorldDir;
        if (PC->DeprojectMousePositionToWorld(MouseWorldLoc, MouseWorldDir))
        {
            // 基本の手の位置
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
                    // ==============================================================
                    // ★【修正】麺の先端をマウスカーソル（箸先）にピッタリ追従させる
                    // ==============================================================
                    FVector VectorToTarget = CubeTargetLoc - NoodleSpawnBaseLocation;
                    float Distance = VectorToTarget.Size();

                    if (Distance > 1.0f)
                    {
                        // 麺の中間地点を計算して配置
                        FVector MidPoint = NoodleSpawnBaseLocation + (VectorToTarget * 0.5f);
                        StretchingNoodleComp->SetWorldLocation(MidPoint);

                        // 麺の向きをカーソルへ向ける
                        FRotator NewRot = FRotationMatrix::MakeFromZ(VectorToTarget).Rotator();
                        StretchingNoodleComp->SetWorldRotation(NewRot);

                        // 距離に合わせて麺を引き伸ばす
                        float MeshHeight = StretchingNoodleComp->GetStaticMesh()->GetBoundingBox().GetSize().Z;
                        if (MeshHeight <= 0.1f) MeshHeight = 100.0f;

                        float StretchScaleZ = Distance / MeshHeight;
                        StretchingNoodleComp->SetWorldScale3D(FVector(NoodleOriginalScale.X, NoodleOriginalScale.Y, StretchScaleZ));
                    }
                }
                else if (PhysicsHandle->GrabbedComponent)
                {
                    PhysicsHandle->SetTargetLocation(CubeTargetLoc);
                }
            }

            // 手の移動処理（素直にマウスカーソルに追従します）
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

void AHAND::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &AHAND::Grab);
    PlayerInputComponent->BindAction("Grab", IE_Released, this, &AHAND::Release);
    PlayerInputComponent->BindAxis("MoveUp", this, &AHAND::MoveUp);
}

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
            // お椀の底からピッタリ生成する
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

                    FVector CamLoc = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
                    CubeTargetDistance = FVector::Dist(NoodleSpawnBaseLocation, CamLoc);
                    LastCubeDistance = CubeTargetDistance;
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

void AHAND::Release()
{
    bIsGrabbing = false;

    // クリックを離したときに麺を消去する
    if (StretchingNoodleComp)
    {
        AActor* NoodleActor = StretchingNoodleComp->GetOwner();

        if (NoodleActor)
        {
            NoodleActor->Destroy();
        }

        StretchingNoodleComp = nullptr;
    }

    if (PhysicsHandle->GrabbedComponent)
    {
        PhysicsHandle->ReleaseComponent();
    }
}

void AHAND::MoveUp(float Value)
{
    if (bIsGameOver) return;

    if (Value != 0.0f)
    {
        CubeTargetDistance += Value * 150.0f;

        if (bIsGrabbing)
        {
            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);
        }
    }
}