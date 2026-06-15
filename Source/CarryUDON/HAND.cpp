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
// コンストラクタ（クラス生成時に一度だけ呼ばれる初期化処理）
// 各種コンポーネントの作成や、デフォルトのモデル・マテリアルを設定します
// ==========================================================
AHAND::AHAND()
{
    // 毎フレーム処理（Tick）を有効にする
    PrimaryActorTick.bCanEverTick = true;

    // ルートコンポーネント（基準点）の作成
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    // 手を表示するためのスタティックメッシュコンポーネントを作成
    HandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HandMesh"));
    HandMesh->SetupAttachment(SceneRoot);

    // デフォルトのメッシュとしてCube（立方体）を読み込み、細長い形にスケール調整
    static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshAsset(TEXT("/Engine/BasicShapes/Cube"));
    if (MeshAsset.Succeeded())
    {
        HandMesh->SetStaticMesh(MeshAsset.Object);
        HandMesh->SetRelativeScale3D(FVector(0.1f, 0.1f, 2.0f));
    }

    // 手自体は物理演算で落下しないようにし、他のオブジェクトとの重なり（Overlap）を検知するように設定
    HandMesh->SetSimulatePhysics(false);
    HandMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    // 物理オブジェクトを掴むためのコンポーネント（PhysicsHandle）を作成
    PhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PhysicsHandle"));

    // カレーが跳ねた時の画面エフェクト用のポストプロセスコンポーネントを作成
    PostProcessComponent = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcessComponent"));
    PostProcessComponent->SetupAttachment(RootComponent);
    PostProcessComponent->bUnbound = true; // 画面全体にエフェクトを適用する設定

    // カレーエフェクト用のマテリアルを読み込む
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> CurryMatAsset(TEXT("Material'/Game/seisaku/curry.curry'"));
    if (CurryMatAsset.Succeeded())
    {
        CurryScreenMaterial = CurryMatAsset.Object;
    }
}

// ==========================================================
// BeginPlay（ゲーム開始時に一度だけ呼ばれる処理）
// 実際のゲーム用モデルの読み込みや、ゲームの初期ステータスを設定します
// ==========================================================
void AHAND::BeginPlay()
{
    Super::BeginPlay();

    // デフォルトのCubeから、実際の手のモデル（hand1.hand1）に差し替える
    UStaticMesh* NewHandMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/seisaku/Model/hand1.hand1")));
    if (NewHandMesh && HandMesh)
    {
        HandMesh->SetStaticMesh(NewHandMesh);
        HandMesh->SetRelativeScale3D(FVector(-1.0f, 1.0f, 1.0f));

        // 手のひらが下を向くように設定
        HandMesh->SetRelativeRotation(FRotator(0.0f, 90.0f, -30.0f));

        // 手の初期相対位置を設定
        HandMesh->SetRelativeLocation(FVector(90.0f, -55.0f, -90.0f));
    }

    // マウスカーソルを画面に表示する
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC) { PC->bShowMouseCursor = true; }

    // 物を掴んだときの物理ハンドルの硬さや減衰（動きの滑らかさ）を設定
    if (PhysicsHandle)
    {
        PhysicsHandle->LinearStiffness = 25000.0f;
        PhysicsHandle->LinearDamping = 500.0f;
        PhysicsHandle->InterpolationSpeed = 100.0f;
    }

    // スコア、残り時間、ゲームオーバー判定の初期化
    CurrentScore = 0;
    CurrentTimeRemaining = GameTimeLimit;
    bIsGameOver = false;

    // カレーエフェクトの透明度を操作できるように、動的マテリアルインスタンスを作成して透明度0（見えない状態）にする
    if (CurryScreenMaterial)
    {
        DynamicCurryMaterial = UMaterialInstanceDynamic::Create(CurryScreenMaterial, this);
        PostProcessComponent->AddOrUpdateBlendable(DynamicCurryMaterial, 1.0f);
        DynamicCurryMaterial->SetScalarParameterValue(FName("Opacity"), 0.0f);
    }
}

// ==========================================================
// Tick（毎フレーム、常に呼ばれ続ける処理）
// ==========================================================
void AHAND::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // ゲームオーバーでなければタイマーを減らす
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

    // 画面左上に残り時間とスコアをデバッグ表示する
    if (GEngine)
    {
        if (!bIsGameOver)
        {
            GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::White, FString::Printf(TEXT("TIME: %.1f"), CurrentTimeRemaining));
            GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow, FString::Printf(TEXT("SCORE: %d"), CurrentScore));
        }
    }

    // マウスカーソルの位置を3D空間上の座標（ワールド座標）に変換する
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (PC)
    {
        FVector MouseWorldLoc, MouseWorldDir;
        if (PC->DeprojectMousePositionToWorld(MouseWorldLoc, MouseWorldDir))
        {
            // ★【修正箇所】掴んでいようがいまいが、手の奥行き（距離）は常に500で固定する！
            // これにより、マウスに合わせて手は動きますが、カメラ側へ近づいてくることはなくなります。
            HandFixedDistance = 500.0f;

            // 手の目標位置と、掴んでいるオブジェクトの目標位置を計算
            FVector HandTargetLoc = MouseWorldLoc + (MouseWorldDir * HandFixedDistance);
            FVector CubeTargetLoc = MouseWorldLoc + (MouseWorldDir * CubeTargetDistance);

            // 何かを掴んでいる場合の処理
            if (bIsGrabbing)
            {
                // マウスを動かした（引っ張った）スピードを計算
                float Speed = FMath::Abs(CubeTargetDistance - LastCubeDistance) / DeltaTime;

                // スピードが閾値（SplashThreshold）を超えたら「カレーが跳ねた」と判定
                if (Speed > SplashThreshold)
                {
                    CurrentCurryAlpha = 1.0f;
                    CurrentScore -= 200;
                    if (CurrentScore < 0) CurrentScore = 0;
                }
                LastCubeDistance = CubeTargetDistance;

                // 麺（StretchingNoodleComp）を掴んでいる場合の伸縮処理
                if (StretchingNoodleComp)
                {
                    FVector VectorToTarget = CubeTargetLoc - NoodleSpawnBaseLocation;
                    float Distance = VectorToTarget.Size();

                    if (Distance > 1.0f)
                    {
                        FVector MidPoint = NoodleSpawnBaseLocation + (VectorToTarget * 0.5f);
                        StretchingNoodleComp->SetWorldLocation(MidPoint);

                        FRotator NewRot = FRotationMatrix::MakeFromZ(VectorToTarget).Rotator();
                        StretchingNoodleComp->SetWorldRotation(NewRot);

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

            // 手のオブジェクト自体を、目標位置へ滑らかに移動させる
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
// Grab（モノを掴む・麺を生成する処理）
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

// ==========================================================
// Release（掴んでいるものを離す処理）
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
// MoveUp（マウスホイール等での奥行き・引っ張り調整処理）
// ==========================================================
void AHAND::MoveUp(float Value)
{
    if (bIsGameOver) return;

    if (Value != 0.0f)
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(10, 0.5f, FColor::Cyan, TEXT("--- Wheel Input Detected! ---"));

        CubeTargetDistance += Value * 150.0f;

        if (bIsGrabbing)
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(11, 0.5f, FColor::Green, TEXT("Status: Grabbing & Suctioning!"));

            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);

            USoundBase* EatSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/eat.eat")));
            if (EatSound)
            {
                UGameplayStatics::PlaySound2D(this, EatSound);
            }

            if (EatSound)
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(12, 0.5f, FColor::Yellow, TEXT("Sound File: FOUND & Playing!"));
                UGameplayStatics::PlaySound2D(this, EatSound);
            }
            else
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(12, 0.5f, FColor::Red, TEXT("Sound File: NOT FOUND! Check your path!"));
            }
        }
    }
}