#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DignityCharacter.h"
#include "DWallJumpComponent.generated.h"

struct FHitResult;
class UDamageType;
class UAnimSequence;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClungToWall, const FHitResult&, ClingHitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnJumpedFromWall);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFellFromWallCling);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWallClingCooldownExpired);

/**
 * This WallMovementComponent describes the ability for the Character it is attached to, to be able to Cling and Jump from acceptable surfaces determined by the parameters outlined in this class.
 *
 * An surface can be Clung to if it is deemed acceptable and the Player possessing the OwningCharacter continues to hold the "Jump" Action key when colliding with the given surface.
 * An Wall Jump can be performed either immediately within an given time frame after colliding with an acceptable surface or after having Clung to an Surface.
 *
 * The OwningCharacters Capsule Component is used to detect "hits" that may result in acceptable Clingable/Jumpable surfaces, the OnCharacterCapsuleHit() function describes this.
 */
UCLASS( ClassGroup = ("Dignity"), HideCategories = ("Variable", "Sockets", "Cooking", "Collision"), meta = (BlueprintSpawnableComponent, DisplayName = "WallMovementComponent") )
class DIGNITY_API UDWallJumpComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UDWallJumpComponent();

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/* Can the Character that owns this WJC Cling to walls. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	uint32 bCanWallCling : 1;

	/* Can the Character that owns this WJC jump from walls. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Jump")
	uint32 bCanWallJump : 1;

	/* Readonly variable that contains if the Character is holding onto an Wall on the right or left side. */
	UPROPERTY(BlueprintReadOnly, Category = " Settings|Cling")
	uint32 bIsCharacterHoldingWallRightSide : 1;

	/* Is the Character required to Cling to the Wall first before they can WallJump. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (EditCondition = "bCanWallCling"), Category = " Settings|Jump")
	uint32 bRequireClingToWallJump : 1;

	/* Enables/Disables the debugging info of WallCling. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = " Settings")
	uint32 bDebugWallCling : 1;

	/* Enables/Disables the debugging info of WallJump. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, AdvancedDisplay, Category = " Settings")
	uint32 bDebugWallJump : 1;

	/* Whether or not to force the Character Mesh to always be perpendicular to the Surface that it is Clinging to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	uint32 bHoldCharacterMeshPerpendicularToSurface : 1;

	/* How long Clinging to an Wall will last before the Character is forced to fall off. Setting this to 0 will disable the effect. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Cling")
	float ClingDuration;

	/* How long to wait for the ability to WallCling again. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Cling")
	float WallJumpInterferenceGracePeriodDuration;

	/**
	 * The Minimum Angle of the Surface normal that the Character can Cling to.
	 * This cannot be greater than ClingMaxPitchSurfaceAngle. 
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, export, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Cling")
	float ClingMinPitchSurfaceAngle;

	/**
	 * The Maximum Angle of the Surface normal that the Character can Cling to. 
	 * This cannot be less than ClingMinPitchSurfaceAngle. 
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, export, meta = (ClampMin = 0, UIMin = 0, ClampMax = 180, UIMax = 180), Category = "Settings|Cling")
	float ClingMaxPitchSurfaceAngle;

	/* The Maximum Forward facing Angle that the Character can approach the Surface from in order to Cling.	*/
	UPROPERTY(BlueprintReadOnly, EditAnywhere, export, meta = (ClampMin = 0, UIMin = 0, ClampMax = 180, UIMax = 180), Category = "Settings|Cling")
	float ClingMaxYawSurfaceAngle;

	/* How much damage needs to be applied in an single hit to force the Character to release an WallCling. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, export, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Cling")
	float ClingDamageReleaseThreshold;

	/* The Maximum number of times this Character can Cling to an wall in an row without landing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	int32 MaxSequentialWallClings;

	/* The Maximum number of WallJumps that the Character can perform in an row without landing. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Jump")
	int32 MaxSequentialWallJumps;

	/* The minimum required velocity the Character must have in order to perform a WallCling. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Cling")
	float MinVelocityToWallCling;

	/* The Look Input Restriction mode to use when Clung to a Wall. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Cling")
	ERestrictLookInput WallClingLookInputRestrictionMode;

	/* The Angle on the Vertical Axis to restrict look input to when Clung to a Wall. These angles will be applied to the view axis defined by WallClingLookInputRestrictionMode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = -90, UIMin = -90, ClampMax = 90, UIMax = 90), Category = "Settings|Cling")
	float LookInputVerticalAngleRestriction;

	/* The Angle on the Horizontal Axis to restrict look input to when Clung to a Wall. These angles will be applied to the view axis defined by WallClingLookInputRestrictionMode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 0, UIMin = 0, ClampMax = 360, UIMax = 360), Category = "Settings|Cling")
	float LookInputHorizontalAngleRestriction;

	/* Controls the power of the WallJump. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = 0, UIMin = 0), Category = "Settings|Jump")
	float WallJumpMagnitude;

	/* The name for the "Jump" Action Input Event. We use this to detect if the Player is holding Jump to Cling to an wall or pressing Jump again to WallJump. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Jump")
	FName JumpActionName;

	/* Delegate that will be called when the Character that owns this WJC clings to an wall. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Clung To Wall"), Category = "Wall Cling")
	FOnClungToWall OnClungToWallDelegate;

	/* Delegate that will be called when the Character that owns this WJC jumps from an wall. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Jumped From Wall"), Category = "Wall Jump")
	FOnJumpedFromWall OnJumpedFromWallDelegate;

	/* Delegate that will be called when the Character that owns this WJC fell from an wall they had clung to. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Fell From Wall"), Category = "Wall Cling")
	FOnFellFromWallCling OnFellFromWallClingDelegate;

	/* Delegate that will be called when the Character that owns this WJC has its WallCling cooldown expire. */
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Wall Cling Cooldown Expired"), Category = "Wall Cling")
	FOnWallClingCooldownExpired OnWallClingCooldownExpiredDelegate;

	/* The animation to play when Clinging to the Left. This is only an convenience variable, this component doesnt play any Anims. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	UAnimSequence* ClingAnimLeft;

	/* The animation to play when Clinging to the Right. This is only an convenience variable, this component doesnt play any Anims. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	UAnimSequence* ClingAnimRight;

	/* The animation to play when not Clinging to the Wall perpendicular but holding the Mesh forward. This is only an convenience variable, this component doesnt play any Anims. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = " Settings|Cling")
	UAnimSequence* ClingAnimForward;

	/* Returns if this Character that owns the WJC is currently clinging to an Wall. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Wall Cling")
	FORCEINLINE bool IsClungToWall() { return bIsClungToWall; }

	/**
	 * Returns how long the Character that owns this WJC has left before they will fall from the wall they are clinging to.
	 *
	 * @return	This returns -1 if they are not currently clinging to an wall.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Wall Cling")
	float RemainingClingTime();

protected:

	/* The Character that owns this WJC. */
	ACharacter* OwningCharacter;

	/* The HitResult returned from the Character Capsule Component we use to determine if its an viable Cling surface. */
	FHitResult WallClingHitResult;

	/* The current amount of WallClings that have been performed in an row without landing. */
	UPROPERTY(Replicated)
	int32 CurrentWallClingCount;

	/* The current amount of WallJumps that have been performed in an row without landing. */
	UPROPERTY(Replicated)
	int32 CurrentWallJumpCount;

	/* True if the Character has just WallJumped, this is used to stop us from Clinging to the same wall in the same frame. */
	UPROPERTY(Replicated)
	bool bAttemptedWallJump;

	/* Called when the OwningCharacters Collision Capsule registers an Hit event. */
	UFUNCTION()
	virtual void OnCharacterCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/* Called when the OwningCharacter has Landed on an Walkable surface. */
	UFUNCTION()
	virtual void OnCharacterLanded(const FHitResult& Hit);

	/* Called when the OwningCharacter has taken any damage, used to check if we need to Uncling from an wall. */
	UFUNCTION()
	virtual void OnCharacterTookAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** 
	 * RPC called to the Server in order to perform an WallJump with the provided Velocity. 
	 *
	 * @param	LaunchVelocity	The Velocity to Launch the Character with.
	 * @param	bRightSideJump	True if the surface the Character jumped from was on their RightHand side.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void PerformCharacterWallJump_Server(const FVector& LaunchVelocity, const bool bRightSideJump = true);

	/**
	 * RPC called to the Server in order to Set if the Character is Clinging to an Wall or to release them from an Wall Cling. 
	 * 
	 * @param	bRightSideCling	True if the surface the Character clung to is on the RightHand side.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void PerformCharacterWallCling_Server(const bool bRightSideCling = true);

	/**
	 * Sets up the CharacterMovementComponent to support the WallCling, if passing false it will return the CMC back to its original state and into an Falling movement mode. 
	 *
	 * @param	bEnable	Sets the Mode to enable or disable.
	 */
	void SetClingMovementMode(bool bEnable = true);

	/**
	 * Releases the Character from an WallCling. 
	 *
	 * @param	bFell	Used to tell if the Release was due to the ClingDuration causing the Character to fall or the Character jumped during the Cling.
	 */
	UFUNCTION()
	void ReleaseWallCling(bool bFell = true);

private:

	/* The Timer Handle used to trigger and forced fall from an wall cling. */
	FTimerHandle WallClingTimerHandle;

	/* The Timer Handle used to ensure that an Cling wont occur in the few frames after attempting to WallJump. */
	FTimerHandle WallJumpInterferenceGracePeriodTimerHandle;

	/* Holds the Characters Forward Vector when they Clung to an Wall. */
	FVector CharacterWallClingForwardVector;

	/* Holds the Characters Right Vector when they Clung to an Wall. */
	FVector CharacterWallClingRightVector;

	/* Holds the Characters Up Vector when they Clung to an Wall. */
	FVector CharacterWallClingUpVector;

	/* Temp var for holding the original bUseControllerRotationYaw value before we override it when we Cling. */
	bool bOriginalControllerRotationYawUseBeforeCling;

	/* The original Mesh rotation before Clinging to the surface, we cache this so we can return it to its original value when unclinging. */
	FRotator OriginalActorMeshRotationBeforeCling;

	/* True if this Character is considered to be clinging to an wall, false otherwise. */
	UPROPERTY(Replicated)
	bool bIsClungToWall;

	/* Logic for handling an Wall Jump that isnt after an Cling. */
	void WallJump();

	/* Logic for jumping when already Clung to an wall. */
	void CheckWallClingJump();

	/* Logic for Clinging to an Wall we have hit. */
	void WallCling();

	/* Forcefully aligns the Character mesh to the Cling surface during an WallCling. */
	void AlignCharacterMeshForCling();

	/* Resets bAttemptedWallJump back to false. */
	void ResetAttemptedWallJump();

	/* Sets the PlayerCameraManagers view restrictions based on the WallClingHit Normal. */
	void ApplyWallClingLookInputRestrictions();
};
