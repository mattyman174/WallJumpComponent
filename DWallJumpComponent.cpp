#include "Dignity.h"
#include "DWallJumpComponent.h"
#include "GameFramework/Character.h"
#include "CollisionQueryParams.h"
#include "UnrealNetwork.h"
#include "DUtility.h"

UDWallJumpComponent::UDWallJumpComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SetIsReplicated(true);

	bCanWallCling = true;
	bCanWallJump = true;
	bRequireClingToWallJump = false;

	bDebugWallCling = false;
	bDebugWallJump = false;

	ClingDuration = 5.f;
	ClingMinPitchSurfaceAngle = 44.f;
	ClingMaxPitchSurfaceAngle = 135.f;
	ClingMaxYawSurfaceAngle = 45.f;
	ClingDamageReleaseThreshold = 10.f;
	bHoldCharacterMeshPerpendicularToSurface = true;
	CharacterWallClingForwardVector = FVector::ZeroVector;
	CharacterWallClingRightVector = FVector::ZeroVector;
	CharacterWallClingUpVector = FVector::ZeroVector;
	bIsCharacterHoldingWallRightSide = false;
	MinVelocityToWallCling = 300.f;
	WallClingLookInputRestrictionMode = ERestrictLookInput::RLI_Both;
	LookInputVerticalAngleRestriction = 45.f;
	LookInputHorizontalAngleRestriction = 180.f;

	CurrentWallClingCount = 0;
	CurrentWallJumpCount = 0;
	MaxSequentialWallClings = 1;
	MaxSequentialWallJumps = 1;
	bAttemptedWallJump = false;
	WallJumpMagnitude = 1000.f;
	WallJumpInterferenceGracePeriodDuration = 0.5f;

	JumpActionName = "Jump";

	OwningCharacter = Cast<ACharacter>(GetOwner());

	bOriginalControllerRotationYawUseBeforeCling = OwningCharacter ? OwningCharacter->bUseControllerRotationYaw : true;

	if (OwningCharacter)
	{
		OwningCharacter->GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &UDWallJumpComponent::OnCharacterCapsuleHit);
		OwningCharacter->LandedDelegate.AddDynamic(this, &UDWallJumpComponent::OnCharacterLanded);
		OwningCharacter->OnTakeAnyDamage.AddDynamic(this, &UDWallJumpComponent::OnCharacterTookAnyDamage);
	}
}

void UDWallJumpComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDWallJumpComponent, CurrentWallJumpCount);
	DOREPLIFETIME(UDWallJumpComponent, CurrentWallClingCount);
	DOREPLIFETIME(UDWallJumpComponent, bIsClungToWall);
	DOREPLIFETIME(UDWallJumpComponent, bAttemptedWallJump);
}

void UDWallJumpComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDWallJumpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	CheckWallClingJump();
}

void UDWallJumpComponent::PostLoad()
{
	Super::PostLoad();

	if (!GIsReconstructingBlueprintInstances)
	{
		ClingMaxPitchSurfaceAngle = FMath::Max3(0.f, ClingMaxPitchSurfaceAngle, ClingMinPitchSurfaceAngle);
	}
}

#if WITH_EDITOR
void UDWallJumpComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UDWallJumpComponent, ClingMaxPitchSurfaceAngle))
	{
		ClingMaxPitchSurfaceAngle = FMath::Max3(0.f, ClingMaxPitchSurfaceAngle, ClingMinPitchSurfaceAngle);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UDWallJumpComponent, ClingMinPitchSurfaceAngle))
	{
		ClingMinPitchSurfaceAngle = FMath::Clamp(ClingMinPitchSurfaceAngle, 0.f, ClingMaxPitchSurfaceAngle);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

float UDWallJumpComponent::RemainingClingTime()
{
	if(GetOwner() && IsClungToWall())
	{
		return GetOwner()->GetWorldTimerManager().GetTimerRemaining(WallClingTimerHandle);
	}

	return -1;
}

void UDWallJumpComponent::OnCharacterCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	check(OwningCharacter);

	if (OwningCharacter->Role == ROLE_AutonomousProxy || (OwningCharacter->Role == ROLE_Authority && OwningCharacter->GetRemoteRole() < ROLE_AutonomousProxy))
	{
		// Check if the Hit is an Surface we can Cling to as long as we arent already clinging to an wall and we havent exceeded our maximum number of wall jumps allowed.
		if (OwningCharacter && OwningCharacter->GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling && OtherComp->GetCollisionObjectType() == ECC_WorldStatic)
		{
			// Find out if our Player is still holding down the "Jump" Action key.
			APlayerController* OwningPlayerController = Cast<APlayerController>(OwningCharacter->GetController());
			FKey JumpKey = UDUtility::GetKeysForActionMapping(nullptr, JumpActionName)[0].Key;
			bool bJumpKeyDown = OwningPlayerController ? OwningPlayerController->IsInputKeyDown(JumpKey) : false; // An WallCling is determined to be that if the Player is still holding "Jump" while hitting the Surface.
			bool bJumpJustPressed = OwningPlayerController ? OwningPlayerController->WasInputKeyJustPressed(JumpKey) : false; // An WallJump is determined to be that if the Player is against an surface an presses the "Jump" key.

			if (bCanWallJump && CurrentWallJumpCount < MaxSequentialWallJumps && bJumpJustPressed)
			{
				// If we need an Cling in order to WallJump and we just attempted an WallJump we need to stop that from happening.
				// This will usually default us to an Cling instead since the chance that another Hit occurs in the next couple of frames is highly likely and bJumpJustPressed shouldnt be valid then
				// thus passing through to the WallCling() evaluation.
				if (bRequireClingToWallJump)
				{
					return;
				}

				WallClingHitResult = Hit;

				WallJump();
			}
			else if (bCanWallCling && !IsClungToWall() && CurrentWallClingCount < MaxSequentialWallClings && bJumpKeyDown/* && !bAttemptedWallJump*/)
			{
				WallClingHitResult = Hit;
				
				WallCling();
			}
		}
	}
}

void UDWallJumpComponent::OnCharacterLanded(const FHitResult& Hit)
{
	// Reset our jump and cling counters when we land on an walkable surface.
	CurrentWallJumpCount = 0;
	CurrentWallClingCount = 0;
	bAttemptedWallJump = false;
}

void UDWallJumpComponent::OnCharacterTookAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (IsClungToWall() && Damage >= ClingDamageReleaseThreshold)
	{
		ReleaseWallCling(true);
	}
}

void UDWallJumpComponent::PerformCharacterWallJump_Server_Implementation(const FVector& LaunchVelocity, const bool bRightSideJump /*= true*/)
{
	check(OwningCharacter);

	CurrentWallJumpCount++;
	bAttemptedWallJump = true;
	OwningCharacter->GetWorldTimerManager().SetTimer(WallJumpInterferenceGracePeriodTimerHandle, this, &UDWallJumpComponent::ResetAttemptedWallJump, WallJumpInterferenceGracePeriodDuration, false);

	// If we are already clung to an wall then we need to make sure to reset the CMCs gravity and movement locks.
	if (IsClungToWall())
	{
		ReleaseWallCling(false);
	}

	OwningCharacter->LaunchCharacter(LaunchVelocity, true, true);
	OnJumpedFromWallDelegate.Broadcast();
}

bool UDWallJumpComponent::PerformCharacterWallJump_Server_Validate(const FVector& LaunchVelocity, const bool bRightSideJump /*= true*/)
{
	return true;
}

void UDWallJumpComponent::PerformCharacterWallCling_Server_Implementation(const bool bRightSideCling /*= true*/)
{
	check(OwningCharacter);

	CurrentWallClingCount++;
	bIsClungToWall = true;
	bAttemptedWallJump = false;

	// Dont release an WallCling as this is considered an infinite cling.
	if (ClingDuration > 0)
	{
		FTimerDelegate ReleaseWallClingTimerDelegate;
		ReleaseWallClingTimerDelegate.BindUFunction(this, FName("ReleaseWallCling"), true);
		OwningCharacter->GetWorldTimerManager().SetTimer(WallClingTimerHandle, ReleaseWallClingTimerDelegate, ClingDuration, false);
	}

	SetClingMovementMode(true);
	OnClungToWallDelegate.Broadcast(WallClingHitResult);

	AlignCharacterMeshForCling();
	ApplyWallClingLookInputRestrictions();
}

bool UDWallJumpComponent::PerformCharacterWallCling_Server_Validate(const bool bRightSideCling /*= true*/)
{
	// Good candidate for checking validity would be to pass the CameraLocation and Rotation to the Server and do the Angle checks again.
	// This would have to be timestamped in order to ensure concurrency.
	return true;
}

void UDWallJumpComponent::SetClingMovementMode(bool bEnable /*= true*/)
{
	check(OwningCharacter);

	UCharacterMovementComponent* DefaultCMC = Cast<UCharacterMovementComponent>(OwningCharacter->GetCharacterMovement()->GetClass()->GetDefaultObject());

	if (bEnable)
	{
		OwningCharacter->GetCharacterMovement()->DisableMovement();
		OwningCharacter->GetCharacterMovement()->GravityScale = 0;
		OwningCharacter->GetCharacterMovement()->Velocity = FVector::ZeroVector;
		bIsClungToWall = true;
	}
	else
	{
		OwningCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		OwningCharacter->GetCharacterMovement()->GravityScale = DefaultCMC ? DefaultCMC->GravityScale : 1.f;
		bIsClungToWall = false;
	}
}

void UDWallJumpComponent::ReleaseWallCling(bool bFell /*= true*/)
{
	check(OwningCharacter);

	SetClingMovementMode(false);

	// Invalidate the WallClingTimerHandle since we are already releasing from the Cling.
	OwningCharacter->GetWorldTimerManager().SetTimer(WallClingTimerHandle, nullptr, -1.f, false);

	if (bFell)
	{
		OnFellFromWallClingDelegate.Broadcast();
	}

	// When we initially Clung we hold the Characters Rotation in place and ignore user input, so we need to set it back to its original value.
	OwningCharacter->SetActorRotation(OriginalActorMeshRotationBeforeCling);
	OwningCharacter->bUseControllerRotationYaw = bOriginalControllerRotationYawUseBeforeCling;

	// Reset any Look Input Restrictions that may have been applied.
	ADignityCharacter* DignityCharacter = Cast<ADignityCharacter>(OwningCharacter);
	if (DignityCharacter)
	{
		DignityCharacter->RestrictCameraViewAngles(ERestrictLookInput::RLI_None, FVector2D::ZeroVector, FVector2D::ZeroVector);
	}
}

void UDWallJumpComponent::WallJump()
{
	check(OwningCharacter);

	FHitResult Hit = WallClingHitResult;
	FVector RicochetVelocity = FVector::ZeroVector;

	APlayerController* OwningPlayerController = Cast<APlayerController>(OwningCharacter->GetController());
	if (OwningPlayerController)
	{
		APlayerCameraManager* OwningPlayerCameraManager = OwningPlayerController->PlayerCameraManager;
		if (OwningPlayerCameraManager)
		{
			FVector Start = OwningPlayerCameraManager->GetCameraLocation();
			FVector End = Start + (OwningPlayerCameraManager->GetCameraRotation().Vector() * WallJumpMagnitude);
			RicochetVelocity = (End - Start).MirrorByVector(Hit.ImpactNormal);
		}
	}

	// Tell the Server we are jumping and update the jump information locally if needed.
	PerformCharacterWallJump_Server(RicochetVelocity);
	if (OwningCharacter->Role <= ROLE_AutonomousProxy)
	{
		OnJumpedFromWallDelegate.Broadcast();
		CurrentWallJumpCount++;
		bAttemptedWallJump = true;
		OwningCharacter->GetWorldTimerManager().SetTimer(WallJumpInterferenceGracePeriodTimerHandle, this, &UDWallJumpComponent::ResetAttemptedWallJump, WallJumpInterferenceGracePeriodDuration, false);
	}

#if !UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
	if (bDebugWallJump)
	{
		FVector RicochetDirection = RicochetVelocity;
		RicochetDirection.Normalize();
		RicochetDirection = (Hit.ImpactPoint + Hit.Normal) + (RicochetDirection * 100.f);

		DrawDebugDirectionalArrow(GetWorld(), Hit.ImpactPoint, RicochetDirection, 5.f, FColor::Red, true, -1.f, 0.f, 1.f);

		DrawDebugString(GetWorld(), Hit.ImpactPoint, "Jump Velocity: " + RicochetVelocity.ToString(), NULL, FColor::Red, -1.f, true);
	}
#endif //!UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
}

void UDWallJumpComponent::CheckWallClingJump()
{
	if (OwningCharacter && OwningCharacter->Role == ROLE_AutonomousProxy || (OwningCharacter->Role == ROLE_Authority && OwningCharacter->GetRemoteRole() < ROLE_AutonomousProxy))
	{
		if (IsClungToWall() && bCanWallJump)
		{
			// Hack for detecting if we are Jumping while in an Cling, unfortunately ActionMappings are single bind delegates and wont allow multiple subscribers so we need to detect this ourselves.
			// If the Character class exposed an OnJumped delegate then this wouldnt be an issue but it doesnt so it is.
			APlayerController* OwningPlayerController = Cast<APlayerController>(OwningCharacter->GetController());
			FKey JumpKey = UDUtility::GetKeysForActionMapping(nullptr, JumpActionName)[0].Key;
			bool bJumpJustPressed = OwningPlayerController ? OwningPlayerController->WasInputKeyJustPressed(JumpKey) : false; // An WallJump can occur from an Cling position if we press "Jump" Action Key.

			if (OwningPlayerController && bJumpJustPressed)
			{
				APlayerCameraManager* OwningPlayerCameraManager = OwningPlayerController->PlayerCameraManager;
				if (OwningPlayerCameraManager)
				{
					FVector Start = OwningPlayerCameraManager->GetCameraLocation();
					FVector End = Start + (OwningPlayerCameraManager->GetCameraRotation().Vector() * WallJumpMagnitude);
					End = (End - OwningCharacter->GetActorLocation());

					// Tell the Server we are jumping and update the jump information locally if needed.
					PerformCharacterWallJump_Server(End);
					if (OwningCharacter->Role <= ROLE_AutonomousProxy)
					{
						CurrentWallJumpCount++;
						bAttemptedWallJump = true;
						ReleaseWallCling(false);
						OnJumpedFromWallDelegate.Broadcast();
					}

#if !UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
					if (bDebugWallJump)
					{
						FVector RicochetDirection = End;
						RicochetDirection.Normalize();
						RicochetDirection = OwningCharacter->GetActorLocation() + (RicochetDirection * 100.f);

						DrawDebugDirectionalArrow(GetWorld(), OwningCharacter->GetActorLocation(), RicochetDirection, 5.f, FColor::Green, true, -1.f, 0.f, 1.f);

						DrawDebugString(GetWorld(), OwningCharacter->GetActorLocation(), "Jump Velocity: " + RicochetDirection.ToString(), NULL, FColor::Green, -1.f, true);
					}
#endif //!UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
				}
			}
		}
	}
}

void UDWallJumpComponent::WallCling()
{
	check(OwningCharacter);

	const FHitResult Hit = WallClingHitResult;
	const FVector InverseSurfaceNormal = Hit.ImpactNormal * -1.f;

	CharacterWallClingForwardVector = OwningCharacter->GetActorForwardVector();
	CharacterWallClingRightVector = OwningCharacter->GetActorRightVector();
	CharacterWallClingUpVector = OwningCharacter->GetActorUpVector();

	// Calculate that the approach angle of the Character and the Surface angle are within the appropriate tolerances
	const float ClingPitchImpactDot = FVector::DotProduct(CharacterWallClingUpVector, Hit.ImpactNormal);
	const float ClingPitchImpactDegree = (180.f) / PI * FMath::Acos(ClingPitchImpactDot);
	const float ClingYawImpactDot = FVector::DotProduct(CharacterWallClingForwardVector, InverseSurfaceNormal);
	const float ClingYawImpactDegree = (180.f) / PI * FMath::Acos(CharacterWallClingForwardVector.CosineAngle2D(InverseSurfaceNormal));

	// Before we use it make sure our ClingMaxPitch isnt exceeding our Min required value or its Max value.
	ClingMaxPitchSurfaceAngle = FMath::Clamp(ClingMaxPitchSurfaceAngle, ClingMinPitchSurfaceAngle, 180.f);

	// If the calculated angles are within our tolerances then we have an valid surface.
	const bool bIsClingPitchImpactDegreeValid = UDUtility::FloatInRange(ClingPitchImpactDegree, ClingMinPitchSurfaceAngle, ClingMaxPitchSurfaceAngle, true, true);
	const bool bIsClingYawImpactDegreeValid = ClingYawImpactDegree <= ClingMaxYawSurfaceAngle;

	const float CharacterVelocity = OwningCharacter->GetVelocity().Size();

	if (bIsClingPitchImpactDegreeValid && bIsClingYawImpactDegreeValid && (CharacterVelocity >= MinVelocityToWallCling) && !bAttemptedWallJump)
	{
		// Tell the Server we are clinging and update cling information locally if needed.
		PerformCharacterWallCling_Server();
		if (OwningCharacter->Role <= ROLE_AutonomousProxy)
		{
			CurrentWallClingCount++;
			bIsClungToWall = true;
			OnClungToWallDelegate.Broadcast(WallClingHitResult);

			AlignCharacterMeshForCling();
			ApplyWallClingLookInputRestrictions();

			//Locally apply the cling timer so we can get the OnFell broadcast.
			if (ClingDuration > 0)
			{
				FTimerDelegate ReleaseWallClingTimerDelegate;
				ReleaseWallClingTimerDelegate.BindUFunction(this, FName("ReleaseWallCling"), true);
				OwningCharacter->GetWorldTimerManager().SetTimer(WallClingTimerHandle, ReleaseWallClingTimerDelegate, ClingDuration, false);
			}
		}
	}

#if !UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
	if (bDebugWallCling)
	{
		FString ClingPitchImpactDegreeString = "Pitch: " + FString::SanitizeFloat(ClingPitchImpactDegree);
		FString ClingImpactYawDegreeString = "Yaw: " + FString::SanitizeFloat(ClingYawImpactDegree);
		FString ClingIsValidString = (bIsClingPitchImpactDegreeValid && bIsClingYawImpactDegreeValid) ? "Valid Cling Surface" : "Invalid Cling Surface";
		FVector ImpactPointExtended = (Hit.ImpactNormal * 50.f) + Hit.ImpactPoint;
		FVector ImpactPointExtendedUp = FVector(ImpactPointExtended.X, ImpactPointExtended.Y, ImpactPointExtended.Z + 50.f);
		FVector CharacterForwardVectorExtended = (CharacterWallClingForwardVector * 50.f) + ImpactPointExtended;
		FVector InverseSurfaceNormalExtended = (InverseSurfaceNormal * 50.f) + Hit.ImpactPoint;

		DrawDebugLine(GetWorld(), Hit.ImpactPoint, ImpactPointExtended, FColor::Green, true, -1.f, 0.f, .5f);
		DrawDebugLine(GetWorld(), ImpactPointExtended, ImpactPointExtendedUp, FColor::Blue, true, -1.f, 0.f, .5f);
		DrawDebugLine(GetWorld(), ImpactPointExtended, CharacterForwardVectorExtended, FColor::Red, true, -1.f, 0.f, .5f);
		DrawDebugLine(GetWorld(), Hit.ImpactPoint, InverseSurfaceNormalExtended, FColor::Yellow, true, -1.f, 0.f, .5f);

		DrawDebugString(GetWorld(), ImpactPointExtended, ClingPitchImpactDegreeString, NULL, FColor::Green, -1.f, true);
		DrawDebugString(GetWorld(), CharacterForwardVectorExtended, ClingImpactYawDegreeString, NULL, FColor::Red, -1.f, true);
		DrawDebugString(GetWorld(), ImpactPointExtendedUp, ClingIsValidString, NULL, FColor::Blue, -1.f, true);
		DrawDebugString(GetWorld(), InverseSurfaceNormalExtended, ("Inverse Normal: " + InverseSurfaceNormal.ToString()), NULL, FColor::Yellow, -1.f, true);

		DrawDebugCircle(GetWorld(), ImpactPointExtended, 25.f, 50.f, FColor::Red, true, -1.f, 0.f, .5f, CharacterWallClingRightVector, CharacterWallClingForwardVector, true);
		DrawDebugCircle(GetWorld(), ImpactPointExtended, 25.f, 50.f, FColor::Green, true, -1.f, 0.f, .5f, FVector::CrossProduct(CharacterWallClingRightVector, InverseSurfaceNormal), InverseSurfaceNormal, true);
	}
#endif //!UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
}

void UDWallJumpComponent::AlignCharacterMeshForCling()
{
	check(OwningCharacter);

	if (IsClungToWall())
	{
		FVector SurfaceNormalUpVector;
		FVector SurfaceNormalForwardVector = WallClingHitResult.ImpactNormal;
		FVector SurfaceNormalLeftVector;

		// Grab the Surface normal axes vectors so we can use them to figure out what side the Character is facing.
		WallClingHitResult.ImpactNormal.FindBestAxisVectors(SurfaceNormalUpVector, SurfaceNormalLeftVector);

		FVector SurfaceNormalRightVector = SurfaceNormalLeftVector * -1.f;

		// Calculate the angle between the ForwardVector when we hit the surface and that of the Right and Left vectors of the surface normal
		// so we can determine what side the Character is facing.
		float CharacterForwardVectorDot = FVector::DotProduct(CharacterWallClingForwardVector, SurfaceNormalRightVector);
		float CharacterRightSideNormalAngle = (180.f) / PI * FMath::Acos(CharacterWallClingForwardVector.CosineAngle2D(SurfaceNormalRightVector));
		float CharacterLeftSideNormalAngle = (180.f) / PI * FMath::Acos(CharacterWallClingForwardVector.CosineAngle2D(SurfaceNormalLeftVector));

		bIsCharacterHoldingWallRightSide = UDUtility::FloatInRange(CharacterRightSideNormalAngle, ClingMaxYawSurfaceAngle, 90.f, true, true);

		// Hold the Character in the Rotation that is parallel to the Surface we Clung to or the against the SurfaceNormal.
		// This is released in ReleaseCling().
		OriginalActorMeshRotationBeforeCling = OwningCharacter->GetActorRotation();
		OwningCharacter->SetActorRotation(bHoldCharacterMeshPerpendicularToSurface ? (bIsCharacterHoldingWallRightSide ? SurfaceNormalRightVector.Rotation() : SurfaceNormalLeftVector.Rotation()) : SurfaceNormalForwardVector.Rotation());
		bOriginalControllerRotationYawUseBeforeCling = OwningCharacter->bUseControllerRotationYaw;
		OwningCharacter->bUseControllerRotationYaw = false;


#if !UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
		if (bDebugWallCling)
		{
			FVector ImpactPointExtended = WallClingHitResult.ImpactPoint + (WallClingHitResult.ImpactNormal * 50.f);
			ImpactPointExtended.Z = ImpactPointExtended.Z + 50.f;

			FVector SurfaceNormalUpExtended = ImpactPointExtended + (SurfaceNormalUpVector * 50.f);
			FVector SurfaceNormalLeftExtended = ImpactPointExtended + (SurfaceNormalLeftVector * 50.f);
			FVector SurfaceNormalRightExtended = ImpactPointExtended + (SurfaceNormalRightVector * 50.f);

			DrawDebugLine(GetWorld(), ImpactPointExtended, SurfaceNormalUpExtended, FColor::Blue, true, -1.f, 0.f, .5f);
			DrawDebugLine(GetWorld(), ImpactPointExtended, SurfaceNormalLeftExtended, FColor::Green, true, -1.f, 0.f, .5f);
			DrawDebugLine(GetWorld(), ImpactPointExtended, SurfaceNormalRightExtended, FColor::Red, true, -1.f, 0.f, .5f);

			DrawDebugString(GetWorld(), SurfaceNormalUpExtended, "Normal Up", NULL, FColor::Blue, -1.f, true);
			DrawDebugString(GetWorld(), SurfaceNormalLeftExtended, "Normal Left: " + FString::SanitizeFloat(CharacterLeftSideNormalAngle) + " | " + (!bIsCharacterHoldingWallRightSide ? "True" : "False"), NULL, FColor::Green, -1.f, true);
			DrawDebugString(GetWorld(), SurfaceNormalRightExtended, "Normal Right: " + FString::SanitizeFloat(CharacterRightSideNormalAngle) + " | " + (bIsCharacterHoldingWallRightSide ? "True" : "False"), NULL, FColor::Red, -1.f, true);
		}
#endif //!UE_BUILD_SHIPPING && DIGNITY_DEVELOPMENT
	}
}

void UDWallJumpComponent::ResetAttemptedWallJump()
{
	bAttemptedWallJump = false;
	OnWallClingCooldownExpiredDelegate.Broadcast();
}

void UDWallJumpComponent::ApplyWallClingLookInputRestrictions()
{
	check(OwningCharacter);

	// We dont need to bother calculating restrictions if they wont ever be applied or they exceed sensible default constraints.
	if (WallClingLookInputRestrictionMode == ERestrictLookInput::RLI_None || (LookInputVerticalAngleRestriction < -90.f || LookInputVerticalAngleRestriction > 90.f) || (LookInputHorizontalAngleRestriction < 0.f || LookInputHorizontalAngleRestriction > 360.f))
	{
		return;
	}

	ADignityCharacter* DignityCharacter = Cast<ADignityCharacter>(OwningCharacter);
	if(DignityCharacter && IsClungToWall())
	{
		const FVector SurfaceNormal = WallClingHitResult.Normal.GetSafeNormal();

		FVector2D ConstrainedPitchDegrees;

		ConstrainedPitchDegrees.X = -90.f + LookInputVerticalAngleRestriction;
		ConstrainedPitchDegrees.Y = 90.f - LookInputVerticalAngleRestriction;

		FVector2D ConstrainedYawDegrees;

		float NegativeSin = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FVector(-0.707f, 0.f, 0.f).GetSafeNormal(), SurfaceNormal)));
		float PositiveSin = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(SurfaceNormal, FVector(0.707f, 0.f, 0.f).GetSafeNormal())));

		ConstrainedYawDegrees.X = SurfaceNormal.Y > 0.f ? PositiveSin + (LookInputHorizontalAngleRestriction / 2.f) + 180.f : NegativeSin + (LookInputHorizontalAngleRestriction / 2.f);
		ConstrainedYawDegrees.Y = SurfaceNormal.Y > 0.f ? PositiveSin - (LookInputHorizontalAngleRestriction / 2.f) + 180.f : NegativeSin - (LookInputHorizontalAngleRestriction / 2.f);

		DignityCharacter->RestrictCameraViewAngles(WallClingLookInputRestrictionMode, ConstrainedPitchDegrees, ConstrainedYawDegrees);
	}
}
