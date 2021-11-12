// Fill out your copyright notice in the Description page of Project Settings.


#include "MainCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Weapon.h"
#include "Sound/SoundCue.h"
#include "Kismet/KismetMathLibrary.h"
#include "Enemy.h"
#include "MainPlayerController.h"
#include "MySaveGame.h"
#include "ItemStorage.h"

// Sets default values
AMainCharacter::AMainCharacter()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create Camera Boom (pulls towards the player if there's a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 500.f;	// Camera follows at this distance
	CameraBoom->bUsePawnControlRotation = true;	// Rotate arm based on controller

	// Set size for collision capsule
	GetCapsuleComponent()->SetCapsuleSize(31.f, 91.f);

	// Create Follow Camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// Attach the camera to the end of the boom and let the boom adjust to match
	// the controller orientation
	FollowCamera->bUsePawnControlRotation = false;

	// Set our turn rates for input
	BaseTurnRate = 65.f;
	BaseLookUpRate = 65.f;

	// Don't rotate when the controller rotates
	// Let that just affect the camera
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;	// Character moves in the direction of input...
	GetCharacterMovement()->RotationRate = FRotator(0.f, 840.f, 0.f);	// ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.2f;

	MaxHealth = 100.f;
	Health = 65.f;
	MaxStamina = 150.f;
	Stamina = 120.f;
	Coins = 0;

	RunningSpeed = 650.f;
	SprintingSpeed = 950.f;

	bShiftKeyDown = false;
	bLMBDown = false;
	bESCDown = false;

	// Initialize Enums
	MovementStatus = EMovementStatus::EMS_Normal;
	StaminaStatus = EStaminaStatus::ESS_Normal;

	StaminaDrainRate = 25.f;
	MinSprintStamina = 50.f;

	InterpSpeed = 15.f;
	bInterpToEnemy = false;

	bHasCombatTarget = false;

	bMovingForward = false;

	bMovingRight = false;
}

// Called when the game starts or when spawned
void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	MainPlayerController = Cast<AMainPlayerController>(GetController());

	FString Map = GetWorld()->GetMapName();
	Map.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

	// this logic works only if you don't wanna come back to the first level after beating it
	if (Map != TEXT("SunTemple"))
	{
		LoadGameNoSwitch();
		if (MainPlayerController)
		{
			MainPlayerController->GameModeOnly();
		}
	}

}

// Called every frame
void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MovementStatus == EMovementStatus::EMS_Dead) return;

	float DeltaStamina = StaminaDrainRate * DeltaTime;

	switch (StaminaStatus)
	{
	case EStaminaStatus::ESS_Normal:
		if (bShiftKeyDown)
		{
			if (Stamina - DeltaStamina <= MinSprintStamina)
			{
				SetStaminaStatus(EStaminaStatus::ESS_BelowMinimum);
				Stamina -= DeltaStamina;
			}
			else if (MovementStatus != EMovementStatus::EMS_Normal)
			{
				Stamina -= DeltaStamina;
			}
			if (bMovingForward || bMovingRight)
			{
				SetMovementStatus(EMovementStatus::EMS_Sprinting);
			}
			else
			{
				SetMovementStatus(EMovementStatus::EMS_Normal);
			}
		}
		else  // Shift key is up
		{
			if (Stamina + DeltaStamina >= MaxStamina)
			{
				Stamina = MaxStamina;
			}
			else
			{
				Stamina += DeltaStamina;
			}
			SetMovementStatus(EMovementStatus::EMS_Normal);
		}
		break;
	case EStaminaStatus::ESS_BelowMinimum:
		if (bShiftKeyDown)
		{
			if (Stamina - DeltaStamina <= 0.f)
			{
				SetStaminaStatus(EStaminaStatus::ESS_Exhausted);
				Stamina = 0.f;
				SetMovementStatus(EMovementStatus::EMS_Normal);
			}
			else if (MovementStatus != EMovementStatus::EMS_Normal)
			{
				Stamina -= DeltaStamina;
				if (bMovingForward || bMovingRight)
				{
					SetMovementStatus(EMovementStatus::EMS_Sprinting);
				}
				else
				{
					SetMovementStatus(EMovementStatus::EMS_Normal);
				}
			}
		}
		else  // Shift key is up
		{
			if (Stamina + DeltaStamina >= MinSprintStamina)
			{
				SetStaminaStatus(EStaminaStatus::ESS_Normal);
				Stamina += DeltaStamina;
			}
			else
			{
				Stamina += DeltaStamina;
			}
			SetMovementStatus(EMovementStatus::EMS_Normal);
		}
		break;
	case EStaminaStatus::ESS_Exhausted:
		if (bShiftKeyDown)
		{
			Stamina = 0.f;
		}
		else  // Shift key is up
		{
			SetStaminaStatus(EStaminaStatus::ESS_ExhaustedRecovering);
			Stamina += DeltaStamina;
		}
		SetMovementStatus(EMovementStatus::EMS_Normal);
		break;
	case EStaminaStatus::ESS_ExhaustedRecovering:
		if (Stamina + DeltaStamina >= MinSprintStamina)
		{
			SetStaminaStatus(EStaminaStatus::ESS_Normal);
			Stamina += DeltaStamina;
		}
		else
		{
			Stamina += DeltaStamina;
		}
		SetMovementStatus(EMovementStatus::EMS_Normal);
		break;
	default:
		;
	}

	if (bInterpToEnemy && CombatTarget)
	{
		FRotator LookAtYaw = GetLookAtRotationYaw(CombatTarget->GetActorLocation());
		FRotator InterpRotation = FMath::RInterpTo(GetActorRotation(), LookAtYaw, DeltaTime, InterpSpeed);

		SetActorRotation(InterpRotation);
	}

	if (CombatTarget)
	{
		CombatTargetLocation = CombatTarget->GetActorLocation();
		if (MainPlayerController)
		{
			MainPlayerController->EnemyLocation = CombatTargetLocation;
		}
	}
}

FRotator AMainCharacter::GetLookAtRotationYaw(FVector Target)
{
	FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), Target);
	FRotator LookAtRotationYaw(0.f, LookAtRotation.Yaw, 0.f);
	return LookAtRotationYaw;
}

// Called to bind functionality to input
void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	check(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AMainCharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &AMainCharacter::ShiftKeyDown);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &AMainCharacter::ShiftKeyUp);

	PlayerInputComponent->BindAction("LMB", IE_Pressed, this, &AMainCharacter::LMBDown);
	PlayerInputComponent->BindAction("LMB", IE_Released, this, &AMainCharacter::LMBUp);

	PlayerInputComponent->BindAction("ESC", IE_Pressed, this, &AMainCharacter::ESCDown);
	PlayerInputComponent->BindAction("ESC", IE_Released, this, &AMainCharacter::ESCUp);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMainCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMainCharacter::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &AMainCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &AMainCharacter::LookUp);

	PlayerInputComponent->BindAxis("TurnRate", this, &AMainCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMainCharacter::LookUpAtRate);
}

bool AMainCharacter::CanMove(float Value)
{
	if (MainPlayerController)
	{
		return (Value != 0.0f) &&
			(!bAttacking) &&
			(MovementStatus != EMovementStatus::EMS_Dead) &&
			(!MainPlayerController->bPauseMenuVisible);
	}

	return false;
}

void AMainCharacter::Turn(float Value)
{
	if (CanMove(Value))
	{
		AddControllerYawInput(Value);
	}
}

void AMainCharacter::LookUp(float Value)
{
	if (CanMove(Value))
	{
		AddControllerPitchInput(Value);
	}
}

void AMainCharacter::MoveForward(float Value)
{
	bMovingForward = false;
	if (CanMove(Value))
	{
		// Find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);

		bMovingForward = true;
	}
}

void AMainCharacter::MoveRight(float Value)
{
	bMovingRight = false;
	if (CanMove(Value))
	{
		// Find our which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(Direction, Value);

		bMovingRight = true;
	}
}

void AMainCharacter::TurnAtRate(float Rate)
{
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMainCharacter::LookUpAtRate(float Rate)
{
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMainCharacter::DecrementHealth(float Amount)
{
	if (Health - Amount <= 0.f)
	{
		Health -= Amount;
		Die();
	}
	else
	{
		Health -= Amount;
	}
}

void AMainCharacter::IncrementCoins(int32 Amount)
{
	Coins += Amount;
}

void AMainCharacter::IncrementHealth(float Amount)
{
	if (Health + Amount >= MaxHealth)
	{
		Health = MaxHealth;
	}
	else
	{
		Health += Amount;
	}
}

void AMainCharacter::Die()
{
	if (MovementStatus == EMovementStatus::EMS_Dead) return;
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && CombatMontage)
	{
		AnimInstance->Montage_Play(CombatMontage, 1.f);
		AnimInstance->Montage_JumpToSection(FName("Death"));
	}

	SetMovementStatus(EMovementStatus::EMS_Dead);
}

void AMainCharacter::Jump()
{
	if (MainPlayerController) if (MainPlayerController->bPauseMenuVisible) return;

	if (MovementStatus != EMovementStatus::EMS_Dead)
	{
		Super::Jump();
	}
}

void AMainCharacter::DeathEnd()
{
	GetMesh()->bPauseAnims = true;
	GetMesh()->bNoSkeletonUpdate = true;
}

void AMainCharacter::SetMovementStatus(EMovementStatus Status)
{
	MovementStatus = Status;
	if (MovementStatus == EMovementStatus::EMS_Sprinting)
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintingSpeed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = RunningSpeed;
	}
}

void AMainCharacter::ShiftKeyDown()
{
	bShiftKeyDown = true;
}

void AMainCharacter::ShiftKeyUp()
{
	bShiftKeyDown = false;
}

void AMainCharacter::LMBDown()
{
	bLMBDown = true;

	if (MovementStatus == EMovementStatus::EMS_Dead) return;

	if (MainPlayerController) if (MainPlayerController->bPauseMenuVisible) return;

	if (ActiveOverlappingItem)
	{
		AWeapon* Weapon = Cast<AWeapon>(ActiveOverlappingItem);
		if (Weapon)
		{
			Weapon->Equip(this);
			SetActiveOverlappedItem(nullptr);
		}
	}
	else if (EquippedWeapon)
	{
		Attack();
	}

}

void AMainCharacter::LMBUp()
{
	bLMBDown = false;
}

void AMainCharacter::ESCDown()
{
	bESCDown = true;

	if (MainPlayerController)
	{
		MainPlayerController->TogglePauseMenu();
	}
}

void AMainCharacter::ESCUp()
{
	bESCDown = false;
}

void AMainCharacter::ShowPickupLocations()
{
	for (int32 i = 0; i < PickupLocations.Num(); i++)	// Or for (FVector[or auto] Location : PickupLocations)
	{
		UKismetSystemLibrary::DrawDebugSphere(this, PickupLocations[i], 25.f, 8, FLinearColor::Green, 10.f, .5f);
	}
}

void AMainCharacter::SetEquippedWeapon(AWeapon* WeaponToSet)
{
	if (EquippedWeapon)
	{
		EquippedWeapon->Destroy();
	}

	EquippedWeapon = WeaponToSet;
}

void AMainCharacter::Attack()
{
	if (!bAttacking && (MovementStatus != EMovementStatus::EMS_Dead))
	{
		bAttacking = true;
		SetInterpToEnemy(true);

		UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
		if (AnimInstance && CombatMontage)
		{
			int32 Section = FMath::RandRange(0, 1);
			switch (Section)
			{
			case 0:
				AnimInstance->Montage_Play(CombatMontage, 1.8f);
				AnimInstance->Montage_JumpToSection(FName("Attack_1"), CombatMontage);
				break;
			case 1:
				AnimInstance->Montage_Play(CombatMontage, 1.5f);
				AnimInstance->Montage_JumpToSection(FName("Attack_2"), CombatMontage);
				break;
			default:
				;
			}
		}
	}
}

void AMainCharacter::AttackEnd()
{
	bAttacking = false;
	SetInterpToEnemy(false);
	if (bLMBDown)
	{
		Attack();
	}
}

void AMainCharacter::PlaySwingSound()
{
	if (EquippedWeapon->SwingSound)
	{
		UGameplayStatics::PlaySound2D(this, EquippedWeapon->SwingSound);
	}
}

void AMainCharacter::SetInterpToEnemy(bool Interp)
{
	bInterpToEnemy = Interp;
}

float AMainCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (Health - DamageAmount <= 0.f)
	{
		Health -= DamageAmount;
		Die();
		if (DamageCauser)
		{
			AEnemy* Enemy = Cast<AEnemy>(DamageCauser);
			if (Enemy)
			{
				Enemy->bHasValidTarget = false;
			}
		}
	}
	else
	{
		Health -= DamageAmount;
	}

	return DamageAmount;
}

void AMainCharacter::UpdateCombatTarget()
{
	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors, EnemyFilter);

	if (OverlappingActors.Num() == 0)
	{
		if (MainPlayerController)
		{
			MainPlayerController->RemoveEnemyHealthBar();
		}

		return;
	}

	AEnemy* ClosestEnemy = Cast<AEnemy>(OverlappingActors[0]);
	if (ClosestEnemy)
	{
		FVector PlayerLocation = GetActorLocation();
		float MinDistance = (ClosestEnemy->GetActorLocation() - PlayerLocation).Size();

		for (auto Actor : OverlappingActors)
		{
			AEnemy* Enemy = Cast<AEnemy>(Actor);
			if (Enemy)
			{
				float DistanceToActor = (Enemy->GetActorLocation() - PlayerLocation).Size();
				if (DistanceToActor < MinDistance)
				{
					MinDistance = DistanceToActor;
					ClosestEnemy = Enemy;
				}
			}
		}

		if (MainPlayerController)
		{
			MainPlayerController->DisplayEnemyHealthBar();
		}

		SetCombatTarget(ClosestEnemy);
		bHasCombatTarget = true;
	}
}

void AMainCharacter::SwitchLevel(FName LevelName)
{
	UWorld* World = GetWorld();
	if (World)
	{
		FString CurrentLevel = World->GetMapName();

		FName CurrentLevelName(*CurrentLevel);
		if (CurrentLevelName != LevelName)
		{
			UGameplayStatics::OpenLevel(World, LevelName);
		}
	}
}

void AMainCharacter::SaveGame()
{
	UMySaveGame* SaveGameInstance = Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));

	SaveGameInstance->CharacterStats.Health = Health;
	SaveGameInstance->CharacterStats.MaxHealth = MaxHealth;
	SaveGameInstance->CharacterStats.Stamina = Stamina;
	SaveGameInstance->CharacterStats.MaxStamina = MaxStamina;
	SaveGameInstance->CharacterStats.Coins = Coins;

	FString MapName = GetWorld()->GetMapName();
	MapName.RemoveFromStart(GetWorld()->StreamingLevelsPrefix);

	SaveGameInstance->CharacterStats.LevelName = MapName;

	if (EquippedWeapon)
	{
		SaveGameInstance->CharacterStats.WeaponName = EquippedWeapon->Name;
	}

	SaveGameInstance->CharacterStats.Location = GetActorLocation();
	SaveGameInstance->CharacterStats.Rotation = GetActorRotation();

	UGameplayStatics::SaveGameToSlot(SaveGameInstance, SaveGameInstance->PlayerName, SaveGameInstance->UserIndex);
}

void AMainCharacter::LoadGame(bool bSetPosition)
{
	UMySaveGame* LoadGameInstance = Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));

	LoadGameInstance = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(LoadGameInstance->PlayerName, LoadGameInstance->UserIndex));

	Health = LoadGameInstance->CharacterStats.Health;
	MaxHealth = LoadGameInstance->CharacterStats.MaxHealth;
	Stamina = LoadGameInstance->CharacterStats.Stamina;
	MaxStamina = LoadGameInstance->CharacterStats.MaxStamina;
	Coins = LoadGameInstance->CharacterStats.Coins;

	if (WeaponStorage)
	{
		AItemStorage* Weapons = GetWorld()->SpawnActor<AItemStorage>(WeaponStorage);
		if (Weapons)
		{
			FString WeaponName = LoadGameInstance->CharacterStats.WeaponName;

			if (Weapons->WeaponMap.Contains(WeaponName))
			{
				AWeapon* WeaponToEquip = GetWorld()->SpawnActor<AWeapon>(Weapons->WeaponMap[WeaponName]);
				WeaponToEquip->Equip(this);
			}
		}
	}

	if (bSetPosition)
	{
		SetActorLocation(LoadGameInstance->CharacterStats.Location);
		SetActorRotation(LoadGameInstance->CharacterStats.Rotation);
	}

	SetMovementStatus(EMovementStatus::EMS_Normal);

	GetMesh()->bPauseAnims = false;
	GetMesh()->bNoSkeletonUpdate = false;

	if (LoadGameInstance->CharacterStats.LevelName != TEXT(""))
	{
		FName LevelName(*LoadGameInstance->CharacterStats.LevelName);

		SwitchLevel(LevelName);
	}
}

void AMainCharacter::LoadGameNoSwitch()
{
	UMySaveGame* LoadGameInstance = Cast<UMySaveGame>(UGameplayStatics::CreateSaveGameObject(UMySaveGame::StaticClass()));

	LoadGameInstance = Cast<UMySaveGame>(UGameplayStatics::LoadGameFromSlot(LoadGameInstance->PlayerName, LoadGameInstance->UserIndex));

	Health = LoadGameInstance->CharacterStats.Health;
	MaxHealth = LoadGameInstance->CharacterStats.MaxHealth;
	Stamina = LoadGameInstance->CharacterStats.Stamina;
	MaxStamina = LoadGameInstance->CharacterStats.MaxStamina;
	Coins = LoadGameInstance->CharacterStats.Coins;

	if (WeaponStorage)
	{
		AItemStorage* Weapons = GetWorld()->SpawnActor<AItemStorage>(WeaponStorage);
		if (Weapons)
		{
			FString WeaponName = LoadGameInstance->CharacterStats.WeaponName;

			if (Weapons->WeaponMap.Contains(WeaponName))
			{
				AWeapon* WeaponToEquip = GetWorld()->SpawnActor<AWeapon>(Weapons->WeaponMap[WeaponName]);
				WeaponToEquip->Equip(this);
			}
		}
	}

	SetMovementStatus(EMovementStatus::EMS_Normal);

	GetMesh()->bPauseAnims = false;
	GetMesh()->bNoSkeletonUpdate = false;
}