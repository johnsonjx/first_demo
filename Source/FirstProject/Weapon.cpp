// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon.h"


#include "Components/SkeletalMeshComponent.h"
#include "Main.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/BoxComponent.h"
#include "Enemy.h"
#include "Engine/SkeletalMeshSocket.h"


AWeapon::AWeapon()
{
	SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
	SkeletalMesh->SetupAttachment(GetRootComponent());

	CombatCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("CombatCollision"));
	CombatCollision->SetupAttachment(GetRootComponent());

	bWeaponParticles = false;

	WeaponState = EWeaponState::EWS_Pickup;

	Damage = 25.f;
}

void AWeapon::BeginPlay()
{
	// This is a call to the original BeginPlay() in the base class, making sure the...
		// ...original functionality there is included here to be used as well.
	Super::BeginPlay();

	CombatCollision->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::CombatOnOverlapBegin);
	CombatCollision->OnComponentEndOverlap.AddDynamic(this, &AWeapon::CombatOnOverlapEnd);

	/* Hard-Coded collision presets */

	// Sets to nothing, but will be toggled on / off via Notifies and functions
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Sets collision object type of box component, makes it something you can move around in the world
	CombatCollision->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	// Sets collision response to ignore all other channels
	CombatCollision->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	// Sets collision to respond to a specific channel in a specific way, in this case overlap with Pawn types...
	// ...since our enemies fall under that type
	CombatCollision->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
}


void AWeapon::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	Super::OnOverlapBegin(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	// Here we verified OtherActor's value, casted/converted it to AMain (our character) type, then...
	// ...created an instance of Main and verified its value, then finally stored the current weapon...
	// ...in our ActiveOverlappingItem variable we made in Main using its setter function. This is only...
	// ...stored temporarily in our ActiveOverlappingItem variable we made, which is returned to having a...
	// ...null value once the character isn't overlapping with the Item anymore.
	if ((WeaponState == EWeaponState::EWS_Pickup) && OtherActor)
	{
		AMain* Main = Cast<AMain>(OtherActor);
		if (Main)
		{
			Main->SetActiveOverlappingItem(this);
		}
	}
}

void AWeapon::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	Super::OnOverlapEnd(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
	// Here we used our setter function for the ActiveOverlappingItem again to set its...
	// ...value to null. This is for if we don't want to/haven't equipped the item, when we...
	// ...stop overlapping with it we'll no longer have an "ActiveOverlappingItem".
	if (OtherActor)
	{
		AMain* Main = Cast<AMain>(OtherActor);
		if (Main)
		{
			Main->SetActiveOverlappingItem(nullptr);
		}
	}
}

void AWeapon::Equip(AMain* Char)
{
	if (Char)
	{
		SetInstigator(Char->GetController());

		// If Char is valid, this keeps the camera from reacting wonky to the weapon
		// Basically the camera will ignore SkeletalMesh, which is the weapon
		SkeletalMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
		// Does the same as above, but for the Pawn (basically our character)
		SkeletalMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);

		// Just in case we're simulating physics, we need to stop doing so to attach the weapon.
		SkeletalMesh->SetSimulatePhysics(false);

		// Created a USkeletalMeshSocket component to store the "RightHandSocket" we added to the skeleton in Unreal.
		// By #including the appropriate .h files, we can bring forth the socket on our character that we need and...
		//...attach the weapon to it.
		const USkeletalMeshSocket* RightHandSocket = Char->GetMesh()->GetSocketByName("RightHandSocket");

		// If RightHandSocket gets and stores the sockt value we need, we can finally attach the weapon to it.
		if (RightHandSocket)
		{
			RightHandSocket->AttachActor(this, Char->GetMesh());

			// Keeps weapon from continuing to rotate while being equipped to player
			// (Inherited from Item class, btw)
			bRotate = false;

			// Once weapon is attached to socket, we access our character and fill that...
			// ...Weapon variable we made with the current weapon using the setter function we made.
			Char->SetEquippedWeapon(this);
			Char->SetActiveOverlappingItem(nullptr);
		}
		if (OnEquipSound)
		{
			UGameplayStatics::PlaySound2D(this, OnEquipSound);
		}
		// Toggles emitting particles on/off. Particles will remain upon pickup if box is checked in Unreal.
		if (!bWeaponParticles)
		{
			IdleParticlesComponent->Deactivate();
		}
	}
}


void AWeapon::CombatOnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult)
{
	if (OtherActor)
	{
		AEnemy* Enemy = Cast<AEnemy>(OtherActor);
		if (Enemy)
		{
			// We want to spawn a particle Emitter at the location of the hit (where socket we created is), but...
			// ...we must verify particles, otherwise Editor will crash.
			if (Enemy->HitParticles)
			{
				// Accessing the socket we created on the skeleton of our weapon. That's where we want...
				// ...the partical emitter to be/spawn.
				const USkeletalMeshSocket* WeaponSocket = SkeletalMesh->GetSocketByName("WeaponSocket");
				if (WeaponSocket)
				{
					FVector SocketLocation = WeaponSocket->GetSocketLocation(SkeletalMesh);
					UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Enemy->HitParticles, SocketLocation, FRotator(0.f), false);
				}
			}
			if (Enemy->HitSound)
			{
				UGameplayStatics::PlaySound2D(this, Enemy->HitSound);
			}
			if (DamageTypeClass)
			{
				UGameplayStatics::ApplyDamage(Enemy, Damage, WeaponInstigator, this, DamageTypeClass);
			}
		}
	}
}


void AWeapon::CombatOnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{

}

void AWeapon::ActivateCollision()
{
	// Sets to not use physics, only overlapping
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}


void AWeapon::DeactivateCollision()
{
	// Sets collision/overlap back to nothing when this function is called
	CombatCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}