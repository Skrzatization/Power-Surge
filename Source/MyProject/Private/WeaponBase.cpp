// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Particles/ParticleSystemComponent.h"
#include "DrawDebugHelpers.h"


// Sets default values
AWeaponBase::AWeaponBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Create weapon mesh
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMesh;

	// Create laser sight
	VisualLaserCone = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualLaserCone"));
	VisualLaserCone->SetupAttachment(WeaponMesh);

	// properties
	WeaponName = "DefaultWeapon";
	AmmoCount = 10000;
	Damage = 10.0f;
	FireRate = 10.0f;
	LastFireTime = 0.0f;
	TimeBetweenShots = 2.0f / FireRate;
	bIsAiming = false;
	BaseRadius = 310.0f;
	MinBaseRadius = 200.0f;
	Height = 1000.0f;
	ConeShrinkSpeed = 5.0f;
	CurrentBaseRadius = BaseRadius;
	MaxRange = 10000.0f;
}

// Called when the game starts or when spawned
void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	VisualLaserCone->SetVisibility(false);
}

// Called every frame
void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsAiming)
	{
		// logical cone sight scaling
		CurrentBaseRadius = FMath::FInterpTo(CurrentBaseRadius, MinBaseRadius, DeltaTime, ConeShrinkSpeed);

		// visual cone sight scaling
		float ScaleX = CurrentBaseRadius / 200.0f;
		float ScaleZ = CurrentBaseRadius / 200.0f;
		float ScaleY = Height / 100.0f;
		VisualLaserCone->SetWorldScale3D(FVector(ScaleX, ScaleY, ScaleZ));
	}
}

void AWeaponBase::Fire()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("FIRE C++")));
	if (!bIsAiming)
	{
		return;
	}

	if (AmmoCount <= 0)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("NO AMMO")));
		if (NoAmmoSound != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(this, NoAmmoSound, GetActorLocation());
		}
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogTemp, Warning, TEXT("CurrentTime: %f, LastFireTime: %f, TimeBetweenShots: %f"), CurrentTime, LastFireTime, TimeBetweenShots);
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("CurrentTime: %f, LastFireTime: %f, TimeBetweenShots: %f, Delta: %f, AmmoCount: %f"), CurrentTime, LastFireTime, TimeBetweenShots, CurrentTime - LastFireTime, AmmoCount));
	if (CurrentTime - LastFireTime > TimeBetweenShots)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("FIRE CHECK TIME")));
		HandleFire();
		LastFireTime = CurrentTime;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle_TimeBetweenShots, this, &AWeaponBase::HandleFire, TimeBetweenShots, false);
	}
}

void AWeaponBase::HandleFire()
{
	PerformHitScan();

	if (MuzzleFlash != nullptr)
	{
		UGameplayStatics::SpawnEmitterAttached(MuzzleFlash, WeaponMesh, "MuzzleFlashSocket");
	}

	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}
	AmmoCount--;
}

FVector AWeaponBase::GenerateRandomPointInBase() const
{
	// Draw random point from cone base
	float RandomAngle = FMath::FRandRange(0.0f, 2.0f * PI);
	float RandomRadius = FMath::FRandRange(0.0f, CurrentBaseRadius);

	float X = RandomRadius * FMath::Cos(RandomAngle);
	float Z = RandomRadius * FMath::Sin(RandomAngle);

	// Return vec in XZ plane
	return FVector(X, Height, Z);
}

void AWeaponBase::PerformHitScan()
{
	UE_LOG(LogTemp, Warning, TEXT("PerformHitScan"));
	FVector MuzzleLocation = WeaponMesh->GetSocketLocation("MuzzleFlashSocket");
	FRotator MuzzleRotation = WeaponMesh->GetSocketRotation("MuzzleFlashSocket");

	// random shot angle
	FVector RandomPointInBase = GenerateRandomPointInBase();

	FVector RandomPoint = MuzzleLocation + MuzzleRotation.RotateVector(FVector(0, Height, 0) + RandomPointInBase);

	FVector ShotDirection = (RandomPoint - MuzzleLocation).GetSafeNormal();
	FVector EndLocation = MuzzleLocation + (ShotDirection * MaxRange);

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;

	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	UE_LOG(LogTemp, Warning, TEXT("PerformHitScan - Before if"));
	if (GetWorld()->LineTraceSingleByChannel(HitResult, MuzzleLocation, EndLocation, ECC_Visibility, QueryParams))
	{
		UE_LOG(LogTemp, Warning, TEXT("PerformHitScan - 1st if 1"));
		//Hits target
		AActor* HitActor = HitResult.GetActor();
		if (HitActor)
		{
			AController* InstigatorController = GetOwner() ? GetOwner()->GetInstigatorController() : nullptr;

			if (InstigatorController && this)
			{
				UGameplayStatics::ApplyPointDamage(HitActor, Damage, ShotDirection, HitResult, InstigatorController, this, nullptr);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("InstigatorController or this == nullptr"));
			}
			
		}
		UE_LOG(LogTemp, Warning, TEXT("PerformHitScan - 1st if 2"));
		//hit debug
		DrawDebugLine(GetWorld(), MuzzleLocation, HitResult.Location, FColor::Red, false, 1.0f, 0, 1.0f);
		DrawDebugPoint(GetWorld(), HitResult.Location, 10.0f, FColor::Red, false, 1.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("PerformHitScan - else"));
		//no hit
		DrawDebugLine(GetWorld(), MuzzleLocation, EndLocation, FColor::Blue, false, 1.0f, 0, 1.0f);
	}
	UE_LOG(LogTemp, Warning, TEXT("PerformHitScan - end"));
}

void AWeaponBase::StartAiming()
{
	bIsAiming = true;
	CurrentBaseRadius = BaseRadius;
	VisualLaserCone->SetVisibility(true);
}

void AWeaponBase::StopAiming()
{
	bIsAiming = false;
	VisualLaserCone->SetVisibility(false);
}