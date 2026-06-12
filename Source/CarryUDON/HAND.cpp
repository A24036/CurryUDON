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

        // ★手のひらが下を向くように、一番右の数値を「90.0f」に変更しました
        HandMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 90.0f));

        // ★【修正】箸の先端を手前の箸（麺）に合わせるための位置ズレ（オフセット）★
        // 画像 image_12.png に基づき、箸先を下(-55.0)に下げ、
        // かつカメラ側(-25.0)に引き、左右の位置(20.0)も微調整しました。
        // これで箸先と箸（麺）オブジェクトが重なるはずです！
        HandMesh->SetRelativeLocation(FVector(20.0f, 55.0f, -55.0f));
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
// マウスへの追従、タイマー処理、麺の伸縮、スコア等の画面表示を行います
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
            // 時間切れになったらゲームオーバー処理（掴んでいるものを離して次のマップへ移行）
            CurrentTimeRemaining = 0.0f;
            bIsGameOver = true;
            Release();
            // スコアを足したい場所（MoveUp関数の中など）で以下のように書きます
            Usukoa* GI = Cast<Usukoa>(GetGameInstance());
            if (GI)
            {
                GI->score = CurrentScore; // 金庫のスコアを直接増やす！
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
                    CurrentCurryAlpha = 1.0f;  // 画面をカレーで汚す
                    CurrentScore -= 200;       // ペナルティとしてスコアを200減らす
                    if (CurrentScore < 0) CurrentScore = 0; // スコアがマイナスにならないようにする
                }
                LastCubeDistance = CubeTargetDistance;

                // 麺（StretchingNoodleComp）を掴んでいる場合の伸縮処理
                if (StretchingNoodleComp)
                {
                    // ==============================================================
                    // ★【修正】麺の先端をマウスカーソル（箸先）にピッタリ追従させる
                    // ==============================================================
                    // 生成地点（どんぶり）からカーソルまでの距離とベクトルを計算
                    FVector VectorToTarget = CubeTargetLoc - NoodleSpawnBaseLocation;
                    float Distance = VectorToTarget.Size();

                    if (Distance > 1.0f)
                    {
                        // 麺の位置を、生成地点とカーソルの「中間地点」に配置する
                        FVector MidPoint = NoodleSpawnBaseLocation + (VectorToTarget * 0.5f);
                        StretchingNoodleComp->SetWorldLocation(MidPoint);

                        // 麺の向き（回転）をカーソルの方向へ向ける
                        FRotator NewRot = FRotationMatrix::MakeFromZ(VectorToTarget).Rotator();
                        StretchingNoodleComp->SetWorldRotation(NewRot);

                        // 距離に合わせて麺のZ軸（縦方向）のスケールを引き伸ばす
                        float MeshHeight = StretchingNoodleComp->GetStaticMesh()->GetBoundingBox().GetSize().Z;
                        if (MeshHeight <= 0.1f) MeshHeight = 100.0f;

                        float StretchScaleZ = Distance / MeshHeight;
                        StretchingNoodleComp->SetWorldScale3D(FVector(NoodleOriginalScale.X, NoodleOriginalScale.Y, StretchScaleZ));
                    }
                }
                // 麺ではなく、普通の物理オブジェクトを掴んでいる場合はそちらを動かす
                else if (PhysicsHandle->GrabbedComponent)
                {
                    PhysicsHandle->SetTargetLocation(CubeTargetLoc);
                }
            }

            // 手のオブジェクト自体を、マウスカーソルの目標位置へ滑らかに移動させる
            SetActorLocation(FMath::VInterpTo(GetActorLocation(), HandTargetLoc, DeltaTime, InterpSpeed));
        }
    }

    // カレーエフェクトが表示されている場合、時間経過とともに徐々に透明（フェードアウト）にする
    if (DynamicCurryMaterial && CurrentCurryAlpha > 0.0f)
    {
        float SlowerFadeSpeed = FadeSpeed * 0.5f;
        CurrentCurryAlpha = FMath::Max(0.0f, CurrentCurryAlpha - SlowerFadeSpeed * DeltaTime);
        DynamicCurryMaterial->SetScalarParameterValue(FName("Opacity"), CurrentCurryAlpha);
    }
}

// ==========================================================
// SetupPlayerInputComponent
// プレイヤーの入力（クリックやスクロール）と、処理（関数）を紐づける
// ==========================================================
void AHAND::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    // "Grab"（左クリック等）を押した時に Grab() を呼ぶ
    PlayerInputComponent->BindAction("Grab", IE_Pressed, this, &AHAND::Grab);
    // "Grab"を離した時に Release() を呼ぶ
    PlayerInputComponent->BindAction("Grab", IE_Released, this, &AHAND::Release);
    // "MoveUp"（マウスホイール等）を動かした時に MoveUp() を呼ぶ
    PlayerInputComponent->BindAxis("MoveUp", this, &AHAND::MoveUp);
}

// ==========================================================
// Grab（モノを掴む・麺を生成する処理）
// ==========================================================
void AHAND::Grab()
{
    if (bIsGameOver) return;

    // 手の周囲に何かしらのオブジェクトがないか、箱状の判定（Box Sweep）を飛ばしてチェックする
    FVector GrabCheckLocation = GetActorLocation();
    FVector BoxSize(100.0f, 100.0f, 100.0f);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this); // 自分自身（手）は当たり判定から除外

    // オブジェクトに当たった場合
    if (GetWorld()->SweepSingleByChannel(Hit, GrabCheckLocation, GrabCheckLocation + FVector(0, 0, 1), FQuat::Identity, ECC_PhysicsBody, FCollisionShape::MakeBox(BoxSize), Params))
    {
        AActor* HitActor = Hit.GetActor();

        // 当たった相手が「Spawner」タグを持っている場合（＝どんぶり等、麺の発生源）
        if (HitActor && HitActor->ActorHasTag(FName("Spawner")))
        {
            // お椀の底（Spawnerの位置）から、新しい麺（AStaticMeshActor）を生成する
            FVector SpawnLocation = HitActor->GetActorLocation();

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            AStaticMeshActor* NewBlock = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), SpawnLocation, HitActor->GetActorRotation(), SpawnParams);

            if (NewBlock)
            {
                // 生成した麺を動かせる状態にする
                NewBlock->GetRootComponent()->SetMobility(EComponentMobility::Movable);
                UStaticMeshComponent* ParentComp = Cast<UStaticMeshComponent>(HitActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));
                UStaticMeshComponent* NewComp = NewBlock->GetStaticMeshComponent();

                // Spawner（親）と同じ見た目・マテリアル・スケールを新しい麺にコピーする
                if (ParentComp && NewComp)
                {
                    NewComp->SetStaticMesh(ParentComp->GetStaticMesh());
                    int32 MatCount = ParentComp->GetNumMaterials();
                    for (int32 i = 0; i < MatCount; i++) { NewComp->SetMaterial(i, ParentComp->GetMaterial(i)); }

                    NoodleOriginalScale = ParentComp->GetComponentScale();
                    NewComp->SetWorldScale3D(NoodleOriginalScale);

                    // 麺は伸び縮みさせるだけで物理落下はさせないためオフにする
                    NewComp->SetSimulatePhysics(false);
                    NewComp->SetCollisionProfileName(TEXT("NoCollision"));

                    // 掴んでいる状態（麺を引っ張っている状態）にする
                    bIsGrabbing = true;
                    StretchingNoodleComp = NewComp;
                    NoodleSpawnBaseLocation = SpawnLocation;

                    // カメラからの距離を記録する
                    FVector CamLoc = GetWorld()->GetFirstPlayerController()->PlayerCameraManager->GetCameraLocation();
                    CubeTargetDistance = FVector::Dist(NoodleSpawnBaseLocation, CamLoc);
                    LastCubeDistance = CubeTargetDistance;
                }
            }
            return; // 麺を生成した場合はここで処理終了
        }

        // Spawner以外の普通の物理オブジェクト（Simulate Physicsがオンのもの）だった場合
        UPrimitiveComponent* HitComp = Hit.GetComponent();
        if (HitComp && HitComp->IsSimulatingPhysics())
        {
            // カメラからの距離を記録して、PhysicsHandleで物理的に掴む
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

    // 麺を引っ張っていた場合、クリックを離した瞬間にその麺を消去（Destroy）する
    if (StretchingNoodleComp)
    {
        AActor* NoodleActor = StretchingNoodleComp->GetOwner();

        if (NoodleActor)
        {
            NoodleActor->Destroy();
        }

        StretchingNoodleComp = nullptr;
    }

    // 普通の物理オブジェクトを掴んでいた場合、PhysicsHandleから離して落下させる
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
        // 【デバッグ1】ホイールの入力自体が届いているかチェック（水色の文字）
        if (GEngine) GEngine->AddOnScreenDebugMessage(10, 0.5f, FColor::Cyan, TEXT("--- Wheel Input Detected! ---"));

        CubeTargetDistance += Value * 150.0f;

        // 何かを掴んで引っ張っている最中であれば、動かした量に応じてスコアを加算する
        if (bIsGrabbing)
        {
            // 【デバッグ2】正しく「掴んだ状態」でホイールを回せているかチェック（緑の文字）
            if (GEngine) GEngine->AddOnScreenDebugMessage(11, 0.5f, FColor::Green, TEXT("Status: Grabbing & Suctioning!"));

            CurrentScore += FMath::CeilToInt(FMath::Abs(Value) * 10.0f);

            // 音ファイルを読み込む
            USoundBase* EatSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/seisaku/Sound/eat.eat")));
            if (EatSound)
            {
                UGameplayStatics::PlaySound2D(this, EatSound);
            }

            if (EatSound)
            {
                // 【デバッグ3】音のデータが正常に見つかった場合（黄色の文字）
                if (GEngine) GEngine->AddOnScreenDebugMessage(12, 0.5f, FColor::Yellow, TEXT("Sound File: FOUND & Playing!"));
                UGameplayStatics::PlaySound2D(this, EatSound);
            }
            else
            {
                // 【デバッグ4】音のデータが見つからなかった場合（赤色の文字）
                if (GEngine) GEngine->AddOnScreenDebugMessage(12, 0.5f, FColor::Red, TEXT("Sound File: NOT FOUND! Check your path!"));
            }
        }
    }
}