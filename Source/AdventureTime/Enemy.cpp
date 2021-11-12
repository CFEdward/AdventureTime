// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "Components/SphereComponent.h"
#include "MainCharacter.h"
#include "AIController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Sound/SoundCue.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "MainPlayerController.h"

// Sets default values
AEnemy::AEnemy()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	AggroSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AggroSphere"));
	AggroSphere->SetupAttachment(GetRootComponent());
	AggroSphere->InitSphereRadius(600.f);
	AggroSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Ignore);

	CombatSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CombatSphere"));
	CombatSphere->SetupAttachment(GetRootComponent());
	CombatSphere->InitSphereRadius(75.f);

	CombatCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatCollision"));
	CombatCollision->SetupAttachment(GetMesh(), FName("EnemySocket"));

	Health = 75.f;
	MaxHealth = 100.f;
	Damage = 10.f;

	AttackMinTime = .5f;
	AttackMaxTime = 3.5f;

	EnemyMovementStatus = EEnemyMovementStatus::EMS_Idle;

	DeathDelay = 3.f;
}

// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	
	AIController = Cast<AAIController>(GetController());

	AggroSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::AggroSphereOnOverlapBegin);
	AggroSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemy::AggroSphereOnOverlapEnd);

	CombatSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatSphereOnOverlapBegin);
	CombatSphere->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatSphereOnOverlapEnd);

	CombatCollision->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::CombatOnOverlapBegin);
	CombatCollision->OnComponentEndOverlap.AddDynamic(this, &AEnemy::CombatOnOverlapEnd);

	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CombatCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	CombatCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CombatCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);

	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);

	bOverlappingCombatSphere = false;

	bHasValidTarget = false;
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

void AEnemy::AggroSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && Alive())
	{
		AMainCharacter* MainCharacter = Cast<AMainCharacter>(OtherActor);

		if (MainCharacter)
		{
			MoveToTarget(MainCharacter);
		}
	}
}

void AEnemy::AggroSphereOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		AMainCharacter* MainCharacter = Cast<AMainCharacter>(OtherActor);

		if (MainCharacter)
		{
			bHasValidTarget = false;

			if (MainCharacter->CombatTarget == this)
			{
				MainCharacter->SetCombatTarget(nullptr);
			}

			MainCharacter->SetHasCombatTarget(false);
			MainCharacter->UpdateCombatTarget();
			SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Idle);

			if (AIController)
			{
				AIController->StopMovement();
			}
		}
	}
}

void AEnemy::CombatSphereOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && Alive())
	{
		AMainCharacter* MainCharacter = Cast<AMainCharacter>(OtherActor);

		if (MainCharacter)
		{
			bHasValidTarget = true;

			MainCharacter->SetCombatTarget(this);
			MainCharacter->SetHasCombatTarget(true);
			MainCharacter->UpdateCombatTarget();

			CombatTarget = MainCharacter;
			bOverlappingCombatSphere = true;
			
			float AttackTime = FMath::FRandRange(AttackMinTime, AttackMaxTime);
			GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
		}
	}
}

void AEnemy::CombatSphereOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor && OtherComp)
	{
		AMainCharacter* MainCharacter = Cast<AMainCharacter>(OtherActor);

		if (MainCharacter)
		{
			bOverlappingCombatSphere = false;
			MoveToTarget(MainCharacter);
			CombatTarget = nullptr;

			if (MainCharacter->CombatTarget == this)
			{
				MainCharacter->SetCombatTarget(nullptr);
				MainCharacter->bHasCombatTarget = false;
				MainCharacter->UpdateCombatTarget();
			}

			if (MainCharacter->MainPlayerController)
			{
				USkeletalMeshComponent* MainMesh = Cast<USkeletalMeshComponent>(OtherComp);
				if (MainMesh) MainCharacter->MainPlayerController->RemoveEnemyHealthBar();
			}

			GetWorldTimerManager().ClearTimer(AttackTimer);
		}
	}
}

void AEnemy::MoveToTarget(AMainCharacter* Target)
{
	SetEnemyMovementStatus(EEnemyMovementStatus::EMS_MoveToTarget);

	if (AIController)
	{
		FAIMoveRequest MoveRequest;
		MoveRequest.SetGoalActor(Target);
		MoveRequest.SetAcceptanceRadius(10.f);

		FNavPathSharedPtr NavPath;
		
		AIController->MoveTo(MoveRequest, &NavPath);
	}
}

void AEnemy::CombatOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		AMainCharacter* MainCharacter = Cast<AMainCharacter>(OtherActor);
		if (MainCharacter)
		{
			if (MainCharacter->HitParticles)
			{
				const USkeletalMeshSocket* TipSocket = GetMesh()->GetSocketByName("TipSocket");
				if (TipSocket)
				{
					FVector SocketLocation = TipSocket->GetSocketLocation(GetMesh());
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MainCharacter->HitParticles, SocketLocation, FRotator(0.f), false);
				}
			}

			if (MainCharacter->HitSound)
			{
				UGameplayStatics::PlaySound2D(this, MainCharacter->HitSound);
			}

			if (DamageTypeClass)
			{
				UGameplayStatics::ApplyDamage(MainCharacter, Damage, AIController, this, DamageTypeClass);
			}
		}
	}
}


void AEnemy::CombatOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

}

void AEnemy::ActivateCollision()
{
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (SwingSound)
	{
		UGameplayStatics::PlaySound2D(this, SwingSound);
	}
}


void AEnemy::DeactivateCollision()
{
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnemy::Attack()
{
	if (Alive() && bHasValidTarget)
	{
		if (AIController)
		{
			AIController->StopMovement();
			SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Attacking);
		}
		if (!bAttacking)
		{
			bAttacking = true;

			UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
			if (AnimInstance)
			{
				AnimInstance->Montage_Play(CombatMontage, 1.35f);
				AnimInstance->Montage_JumpToSection(FName("Attack"), CombatMontage);
			}
		}
	}
}

void AEnemy::AttackEnd()
{
	bAttacking = false;
	if (bOverlappingCombatSphere)
	{
		float AttackTime = FMath::FRandRange(AttackMinTime, AttackMaxTime);
		GetWorldTimerManager().SetTimer(AttackTimer, this, &AEnemy::Attack, AttackTime);
	}
}

float AEnemy::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (Health - DamageAmount <= 0.f)
	{
		Health -= DamageAmount;
		Die(DamageCauser);
	}
	else
	{
		Health -= DamageAmount;
	}

	return DamageAmount;
}

void AEnemy::Die(AActor* DeathCauser)
{
	SetEnemyMovementStatus(EEnemyMovementStatus::EMS_Dead);
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(CombatMontage, 1.35f);
		AnimInstance->Montage_JumpToSection(FName("Death"), CombatMontage);
	}

	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AggroSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CombatSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bAttacking = false;

	AMainCharacter* MainCharacter = Cast<AMainCharacter>(DeathCauser);
	if (MainCharacter)
	{
		MainCharacter->UpdateCombatTarget();
	}
}

void AEnemy::DeathEnd()
{
	GetMesh()->bPauseAnims = true;
	GetMesh()->bNoSkeletonUpdate = true;

	GetWorldTimerManager().SetTimer(DeathTimer, this, &AEnemy::Disappear, DeathDelay);
}

bool AEnemy::Alive()
{
	return GetEnemyMovementStatus() != EEnemyMovementStatus::EMS_Dead;
}

void AEnemy::Disappear()
{
	Destroy();
}