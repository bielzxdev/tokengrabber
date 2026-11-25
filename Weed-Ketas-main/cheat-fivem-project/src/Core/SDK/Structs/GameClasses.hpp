#pragma once

#include <cstring>
#include <algorithm>
#include <array>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#pragma region Enums
enum BoneMasks : int { SKEL_ROOT = 0x0, SKEL_Pelvis = 0x2e28, SKEL_L_Thigh = 0xe39f, SKEL_L_Calf = 0xf9bb, SKEL_L_Foot = 0x3779, SKEL_L_Toe0 = 0x83c, IK_L_Foot = 0xfedd, PH_L_Foot = 0xe175, MH_L_Knee = 0xb3fe, SKEL_R_Thigh = 0xca72, SKEL_R_Calf = 0x9000, SKEL_R_Foot = 0xcc4d, SKEL_R_Toe0 = 0x512d, IK_R_Foot = 0x8aae, PH_R_Foot = 0x60e6, MH_R_Knee = 0x3fcf, RB_L_ThighRoll = 0x5c57, RB_R_ThighRoll = 0x192a, SKEL_Spine_Root = 0xe0fd, SKEL_Spine0 = 0x5c01, SKEL_Spine1 = 0x60f0, SKEL_Spine2 = 0x60f1, SKEL_Spine3 = 0x60f2, SKEL_L_Clavicle = 0xfcd9, SKEL_L_UpperArm = 0xb1c5, SKEL_L_Forearm = 0xeeeb, SKEL_L_Hand = 0x49d9, SKEL_L_Finger00 = 0x67f2, SKEL_L_Finger01 = 0xff9, SKEL_L_Finger02 = 0xffa, SKEL_L_Finger10 = 0x67f3, SKEL_L_Finger11 = 0x1049, SKEL_L_Finger12 = 0x104a, SKEL_L_Finger20 = 0x67f4, SKEL_L_Finger21 = 0x1059, SKEL_L_Finger22 = 0x105a, SKEL_L_Finger30 = 0x67f5, SKEL_L_Finger31 = 0x1029, SKEL_L_Finger32 = 0x102a, SKEL_L_Finger40 = 0x67f6, SKEL_L_Finger41 = 0x1039, SKEL_L_Finger42 = 0x103a, PH_L_Hand = 0xeb95, IK_L_Hand = 0x8cbd, RB_L_ForeArmRoll = 0xee4f, RB_L_ArmRoll = 0x1470, MH_L_Elbow = 0x58b7, SKEL_R_Clavicle = 0x29d2, SKEL_R_UpperArm = 0x9d4d, SKEL_R_Forearm = 0x6e5c, SKEL_R_Hand = 0xdead, SKEL_R_Finger00 = 0xe5f2, SKEL_R_Finger01 = 0xfa10, SKEL_R_Finger02 = 0xfa11, SKEL_R_Finger10 = 0xe5f3, SKEL_R_Finger11 = 0xfa60, SKEL_R_Finger12 = 0xfa61, SKEL_R_Finger20 = 0xe5f4, SKEL_R_Finger21 = 0xfa70, SKEL_R_Finger22 = 0xfa71, SKEL_R_Finger30 = 0xe5f5, SKEL_R_Finger31 = 0xfa40, SKEL_R_Finger32 = 0xfa41, SKEL_R_Finger40 = 0xe5f6, SKEL_R_Finger41 = 0xfa50, SKEL_R_Finger42 = 0xfa51, PH_R_Hand = 0x6f06, IK_R_Hand = 0x188e, RB_R_ForeArmRoll = 0xab22, RB_R_ArmRoll = 0x90ff, MH_R_Elbow = 0xbb0, SKEL_Neck_1 = 0x9995, SKEL_Head = 0x796e, IK_Head = 0x322c, FACIAL_facialRoot = 0xfe2c, FB_L_Brow_Out_000 = 0xe3db, FB_L_Lid_Upper_000 = 0xb2b6, FB_L_Eye_000 = 0x62ac, FB_L_CheekBone_000 = 0x542e, FB_L_Lip_Corner_000 = 0x74ac, FB_R_Lid_Upper_000 = 0xaa10, FB_R_Eye_000 = 0x6b52, FB_R_CheekBone_000 = 0x4b88, FB_R_Brow_Out_000 = 0x54c, FB_R_Lip_Corner_000 = 0x2ba6, FB_Brow_Centre_000 = 0x9149, FB_UpperLipRoot_000 = 0x4ed2, FB_UpperLip_000 = 0xf18f, FB_L_Lip_Top_000 = 0x4f37, FB_R_Lip_Top_000 = 0x4537, FB_Jaw_000 = 0xb4a0, FB_LowerLipRoot_000 = 0x4324, FB_LowerLip_000 = 0x508f, FB_L_Lip_Bot_000 = 0xb93b, FB_R_Lip_Bot_000 = 0xc33b, FB_Tongue_000 = 0xb987, RB_Neck_1 = 0x8b93, IK_Root = 0xdd1c };
enum eFrameFlags : std::uint32_t { eFrameFlagExplosiveAmmo = 1 << 11, eFrameFlagFireAmmo = 1 << 12, eFrameFlagExplosiveMelee = 1 << 13, eFrameFlagSuperJump = 1 << 14, eFrameFlagIncreasePlayerJumpSupressionRange = 1 << 15, eFrameFlagBeastJump = 1 << 28 };
enum class eAmmoSpecialType : int32_t { None, ArmorPiercing, Explosive, FMJ, HollowPoint, Incendiary, Tracer };
enum ePedConfigFlag { NoCriticalHits = 2, DrownsInWater = 3, DisableReticuleFixedLockon = 4, UpperBodyDamageAnimsOnly = 7, NeverLeavesGroup = 13, BlockNonTemporaryEvents = 17, CanPunch = 18, IgnoreSeenMelee = 24, GetOutUndriveableVehicle = 29, CanFlyThruWindscreen = 32, DiesWhenRagdoll = 33, HasHelmet = 34, PutOnMotorcycleHelmet = 35, DontTakeOffHelmet = 36, DisableEvasiveDives = 39, DontInfluenceWantedLevel = 42, DisablePlayerLockon = 43, DisableLockonToRandomPeds = 44, AllowLockonToFriendlyPlayers = 45, BeingDeleted = 47, BlockWeaponSwitching = 48, NoCollision = 52, IsShooting = 58, WasShooting = 59, IsOnGround = 60, WasOnGround = 61, InVehicle = 62, OnMount = 63, AttachedToVehicle = 64, IsSwimming = 65, WasSwimming = 66, IsSkiing = 67, IsSitting = 68, KilledByStealth = 69, KilledByTakedown = 70, Knockedout = 71, IsSniperScopeActive = 72, SuperDead = 73, UsingCoverPoint = 75, IsInTheAir = 76, IsAimingGun = 78, ForcePedLoadCover = 93, VaultFromCover = 97, IsDrunk = 100, ForcedAim = 101, IsNotRagdollAndNotPlayingAnim = 104, Force
	= 105, DontActivateRagdollFromVehicleImpact = 106, DontActivateRagdollFromBulletImpact = 107, DontActivateRagdollFromExplosions = 108, DontActivateRagdollFromFire = 109, DontActivateRagdollFromElectrocution = 110, KeepWeaponHolsteredUnlessFired = 113, GetOutBurningVehicle = 116, BumpedByPlayer = 117, RunFromFiresAndExplosions = 118, TreatAsPlayerDuringTargeting = 119, IsHandCuffed = 120, IsAnkleCuffed = 121, DisableMelee = 122, DisableUnarmedDrivebys = 123, JustGetsPulledOutWhenElectrocuted = 124, NmMessage466 = 125, WillNotHotwireLawEnforcementVehicle = 126, WillCommandeerRatherThanJack = 127, CanBeAgitated = 128, ForcePedToFaceLeftInCover = 129, ForcePedToFaceRightInCover = 130, BlockPedFromTurningInCover = 131, KeepRelationshipGroupAfterCleanUp = 132, ForcePedToBeDragged = 133, PreventPedFromReactingToBeingJacked = 134, IsScuba = 135, WillArrestRatherThanJack = 136, RemoveDeadExtraFarAway = 137, RidingTrain = 138, ArrestResult = 139, CanAttackFriendly = 140, WillJackAnyPlayer = 141, WillJackWantedPlayersRatherThanStealCar = 144, ShootingAnimFlag = 145, DisableLadderClimbing = 146, StairsDetected = 147, SlopeDetected = 148, CowerInsteadOfFlee = 150, CanActivateRagdollWhenVehicleUpsideDown = 151, AlwaysRespondToCriesForHelp = 152, DisableBloodPoolCreation = 153, ShouldFixIfNoCollision = 154, CanPerformArrest = 155, CanPerformUncuff = 156, CanBeArrested = 157, PlayerPreferFrontSeatMP = 159, IsInjured = 166, DontEnterVehiclesInPlayersGroup = 167, PreventAllMeleeTaunts = 169, IsInjured2 = 170, AlwaysSeeApproachingVehicles = 171, CanDiveAwayFromApproachingVehicles = 172, AllowPlayerToInterruptVehicleEntryExit = 173, OnlyAttackLawIfPlayerIsWanted = 174, PedsJackingMeDontGetIn = 177, PedIgnoresAnimInterruptEvents = 179, IsInCustody = 180, ForceStandardBumpReactionThresholds = 181, LawWillOnlyAttackIfPlayerIsWanted = 182, IsAgitated = 183, PreventAutoShuffleToDriversSeat = 184, UseKinematicModeWhenStationary = 185, EnableWeaponBlocking = 186, HasHurtStarted = 187, DisableHurt = 188, PlayerIsWeird = 189, DoNothingWhenOnFootByDefault = 193, UsingScenario = 194, VisibleOnScreen = 195, DontActivateRagdollOnVehicleCollisionWhenDead = 199, HasBeenInArmedCombat = 200, AvoidanceIgnoreAll = 202, AvoidanceIgnoredByAll = 203, AvoidanceIgnoreGroup1 = 204, AvoidanceMemberOfGroup1 = 205, ForcedToUseSpecificGroupSeatIndex = 206, DisableExplosionReactions = 208, DodgedPlayer = 209, WaitingForPlayerControlInterrupt = 210, ForcedToStayInCover = 211, GeneratesSoundEvents = 212, ListensToSoundEvents = 213, AllowToBeTargetedInAVehicle = 214, WaitForDirectEntryPointToBeFreeWhenExiting = 215, OnlyRequireOnePressToExitVehicle = 216, ForceExitToSkyDive = 217, DontEnterLeadersVehicle = 220, DisableExitToSkyDive = 221, Shrink = 223, MeleeCombat = 224, DisablePotentialToBeWalkedIntoResponse = 225, DisablePedAvoidance = 226, ForceRagdollUponDeath = 227, DisablePanicInVehicle = 229, AllowedToDetachTrailer = 230, IsHoldingProp = 236, BlocksPathingWhenDead = 237, ForceSkinCharacterCloth = 240, DisableStoppingVehicleEngine = 241, PhoneDisableTextingAnimations = 242, PhoneDisableTalkingAnimations = 243, PhoneDisableCameraAnimations = 244, DisableBlindFiringInShotReactions = 245, AllowNearbyCoverUsage = 246, CanPlayInCarIdles = 248, CanAttackNonWantedPlayerAsLaw = 249, WillTakeDamageWhenVehicleCrashes = 250, AICanDrivePlayerAsRearPassenger = 251, PlayerCanJackFriendlyPlayers = 252, IsOnStairs = 253, AIDriverAllowFriendlyPassengerSeatEntry = 255, AllowMissionPedToUseInjuredMovement = 257, PreventUsingLowerPrioritySeats = 261, DisableClosingVehicleDoor = 264, TeleportToLeaderVehicle = 268, AvoidanceIgnoreWeirdPedBuffer = 269, OnStairSlope = 270, DontBlipCop = 272, ClimbedShiftedFence = 273, KillWhenTrapped = 275, EdgeDetected = 276, AvoidTearGas = 279, NoWrithe = 281, OnlyUseForcedSeatWhenEnteringHeliInGroup = 282, DisableWeirdPedEvents = 285, ShouldChargeNow = 286, RagdollingOnBoat = 287, HasBrandishedWeapon = 288, FreezePosition = 292, DisableShockingEvents = 294, NeverReactToPedOnRoof = 296, DisableShockingDrivingOnPavementEvents = 299, DisablePedConstraints = 301, ForceInitialPeekInCover = 302, DisableJumpingFromVehiclesAfterLeader = 305, IsInCluster = 310, ShoutToGroupOnPlayerMelee = 311, IgnoredByAutoOpenDoors = 312, NoPedMelee = 314, CheckLoSForSoundEvents = 315, CanSayFollowedByPlayerAudio = 317, ActivateRagdollFromMinorPlayerContact = 318, ForcePoseCharacterCloth = 320, HasClothCollisionBounds = 321, HasHighHeels = 322, DontBehaveLikeLaw = 324, DisablePoliceInvestigatingBody = 326, DisableWritheShootFromGround = 327, LowerPriorityOfWarpSeats = 328, DisableTalkTo = 329, DontBlip = 330, IsSwitchingWeapon = 331, IgnoreLegIkRestrictions = 332, AllowTaskDoNothingTimeslicing = 339, NotAllowedToJackAnyPlayers = 342, AlwaysLeaveTrainUponArrival = 345, OnlyWritheFromWeaponDamage = 347, UseSloMoBloodVfx = 348, EquipJetpack = 349, PreventDraggedOutOfCarThreatResponse = 350, ForceDeepSurfaceCheck = 356, DisableDeepSurfaceAnims = 357, DontBlipNotSynced = 358, IsDuckingInVehicle = 359, PreventAutoShuffleToTurretSeat = 360, DisableEventInteriorStatusCheck = 361, HasReserveParachute = 362, UseReserveParachute = 363, TreatDislikeAsHateWhenInCombat = 364, OnlyUpdateTargetWantedIfSeen = 365, AllowAutoShuffleToDriversSeat = 366, PreventReactingToSilencedCloneBullets = 372, DisableInjuredCryForHelpEvents = 373, NeverLeaveTrain = 374, DontDropJetpackOnDeath = 375, DisableAutoEquipHelmetsInBikes = 380, IsClimbingLadder = 388, HasBareFeet = 389, GoOnWithoutVehicleIfItIsUnableToGetBackToRoad = 391, BlockDroppingHealthSnacksOnDeath = 392, ForceThreatResponseToNonFriendToFriendMeleeActions = 394, DontRespondToRandomPedsDamage = 395, AllowContinuousThreatResponseWantedLevelUpdates = 396, KeepTargetLossResponseOnCleanup = 397, PlayersDontDragMeOutOfCar = 398, BroadcastRepondedToThreatWhenGoingToPointShooting = 399, IgnorePedTypeForIsFriendlyWith = 400, TreatNonFriendlyAsHateWhenInCombat = 401, DontLeaveVehicleIfLeaderNotInVehicle = 402, AllowMeleeReactionIfMeleeProofIsOn = 404, UseNormalExplosionDamageWhenBlownUpInVehicle = 407, DisableHomingMissileLockForVehiclePedInside = 408, DisableTakeOffScubaGear = 409, Alpha = 410, LawPedsCanFleeFromNonWantedPlayer = 411, ForceBlipSecurityPedsIfPlayerIsWanted = 412, IsHolsteringWeapon = 413, UseGoToPointForScenarioNavigation = 414, DontClearLocalPassengersWantedLevel = 415, BlockAutoSwapOnWeaponPickups = 416, ThisPedIsATargetPriorityForAI = 417, IsSwitchingHelmetVisor = 418, ForceHelmetVisorSwitch = 419, FlamingFootprints = 421, DisableVehicleCombat = 422, DisablePropKnockOff = 423, FallsLikeAircraft = 424, UseLockpickVehicleEntryAnimations = 426, IgnoreInteriorCheckForSprinting = 427, SwatHeliSpawnWithinLastSpottedLocation = 428, DisableStartingVehicleEngine = 429, IgnoreBeingOnFire = 430, DisableTurretOrRearSeatPreference = 431, DisableWantedHelicopterSpawning = 432, UseTargetPerceptionForCreatingAimedAtEvents = 433, DisableHomingMissileLockon = 434, ForceIgnoreMaxMeleeActiveSupportCombatants = 435, StayInDefensiveAreaWhenInVehicle = 436, DontShoutTargetPosition = 437, DisableHelmetArmor = 438, PreventVehExitDueToInvalidWeapon = 441, IgnoreNetSessionFriendlyFireCheckForAllowDamage = 442, DontLeaveCombatIfTargetPlayerIsAttackedByPolice = 443, CheckLockedBeforeWarp = 444, DontShuffleInVehicleToMakeRoom = 445, GiveWeaponOnGetup = 446, DontHitVehicleWithProjectiles = 447, DisableForcedEntryForOpenVehiclesFromTryLockedDoor = 448, FiresDummyRockets = 449, IsArresting = 450, IsDecoyPed = 451, HasEstablishedDecoy = 452, BlockDispatchedHelicoptersFromLanding = 453, DontCryForHelpOnStun = 454, CanBeIncapacitated = 456, MutableForcedAim = 457, DontChangeTargetFromMelee = 458 };
enum eVehicleLockState { VEHICLELOCK_NONE = 0, VEHICLELOCK_UNLOCKED = 1, VEHICLELOCK_LOCKED = 2, VEHICLELOCK_LOCKOUT_PLAYER_ONLY = 3, VEHICLELOCK_LOCKED_PLAYER_INSIDE = 4, VEHICLELOCK_LOCKED_INITIALLY = 5, VEHICLELOCK_FORCE_SHUT_DOORS = 6, VEHICLELOCK_LOCKED_BUT_CAN_BE_DAMAGED = 7, VEHICLELOCK_LOCKED_BUT_BOOT_UNLOCKED = 8, VEHICLELOCK_LOCKED_NO_PASSENGERS = 9, VEHICLELOCK_CANNOT_ENTER = 10 };
enum eTempAction { TA_NONE = 0, TA_WAIT = 1, TA_UNUSED = 2, TA_BRAKE_REVERSE = 3, TA_HANDBRAKE_TURN_LEFT = 4, TA_HANDBRAKE_TURN_RIGHT = 5, TA_HANDBRAKE_UNTIL_TIME_ENDS = 6, TA_TURN_LEFT = 7, TA_TURN_RIGHT = 8, TA_ACCELERATE = 9, TA_UNUSED_12 = 12, TA_TURN_LEFT_GO_REVERSE = 13, TA_TURN_RIGHT_GO_REVERSE = 14, TA_PLANE_FLY_UP = 15, TA_PLANE_FLY_STRAIGHT = 16, TA_PLANE_SHARP_LEFT = 17, TA_PLANE_SHARP_RIGHT = 18, TA_STRONG_BRAKE = 19, TA_TURN_LEFT_AND_STOP = 20, TA_TURN_RIGHT_AND_STOP = 21, TA_GO_IN_REVERSE = 22, TA_ACCELERATE_FAST = 23, TA_BRAKE_ACTION = 24, TA_HANDBRAKE_TURN_LEFT_MORE = 25, TA_HANDBRAKE_TURN_RIGHT_MORE = 26, TA_HANDBRAKE_BRAKE_STRAIGHT = 27, TA_BRAKE_STRONG_REVERSE_ACCELERATION = 28, TA_UNUSED_29 = 29, TA_PERFORMS_BURNOUT = 30, TA_REV_ENGINE = 31, TA_ACCELERATE_VERY_STRONG = 32, TA_SURFACE_IN_SUBMARINE = 33 };
enum eVehicleFlags : unsigned long long { FLAG_SMALL_WORKER = 1ULL << 0, FLAG_BIG = 1ULL << 1, FLAG_NO_BOOT = 1ULL << 2, FLAG_ONLY_DURING_OFFICE_HOURS = 1ULL << 3, FLAG_BOOT_IN_FRONT = 1ULL << 4, FLAG_IS_VAN = 1ULL << 5, FLAG_AVOID_TURNS = 1ULL << 6, FLAG_HAS_LIVERY = 1ULL << 7, FLAG_LIVERY_MATCH_EXTRA = 1ULL << 8, FLAG_SPORTS = 1ULL << 9, FLAG_DELIVERY = 1ULL << 10, FLAG_NOAMBIENTOCCLUSION = 1ULL << 11, FLAG_ONLY_ON_HIGHWAYS = 1ULL << 12, FLAG_TALL_SHIP = 1ULL << 13, FLAG_SPAWN_ON_TRAILER = 1ULL << 14, FLAG_SPAWN_BOAT_ON_TRAILER = 1ULL << 15, FLAG_EXTRAS_GANG = 1ULL << 16, FLAG_EXTRAS_CONVERTIBLE = 1ULL << 17, FLAG_EXTRAS_TAXI = 1ULL << 18, FLAG_EXTRAS_RARE = 1ULL << 19, FLAG_EXTRAS_REQUIRE = 1ULL << 20, FLAG_EXTRAS_STRONG = 1ULL << 21, FLAG_EXTRAS_ONLY_BREAK_WHEN_DESTROYED = 1ULL << 22, FLAG_EXTRAS_SCRIPT = 1ULL << 23, FLAG_EXTRAS_ALL = 1ULL << 24, FLAG_EXTRAS_MATCH_LIVERY = 1ULL << 25, FLAG_DONT_ROTATE_TAIL_ROTOR = 1ULL << 26, FLAG_PARKING_SENSORS = 1ULL << 27, FLAG_PEDS_CAN_STAND_ON_TOP = 1ULL << 28, FLAG_TAILGATE_TYPE_BOOT = 1ULL << 29, FLAG_GEN_NAVMESH = 1ULL << 30, FLAG_LAW_ENFORCEMENT = 1ULL << 31, FLAG_EMERGENCY_SERVICE = 1ULL << 32, FLAG_DRIVER_NO_DRIVE_BY = 1ULL << 33, FLAG_NO_RESPRAY = 1ULL << 34, FLAG_IGNORE_ON_SIDE_CHECK = 1ULL << 35, FLAG_RICH_CAR = 1ULL << 36, FLAG_AVERAGE_CAR = 1ULL << 37, FLAG_POOR_CAR = 1ULL << 38, FLAG_ALLOWS_RAPPEL = 1ULL << 39, FLAG_DONT_CLOSE_DOOR_UPON_EXIT = 1ULL << 40, FLAG_USE_HIGHER_DOOR_TORQUE = 1ULL << 41, FLAG_DISABLE_THROUGH_WINDSCREEN = 1ULL << 42, FLAG_IS_ELECTRIC = 1ULL << 43, FLAG_NO_BROKEN_DOWN_SCENARIO = 1ULL << 44, FLAG_IS_JETSKI = 1ULL << 45, FLAG_DAMPEN_STICKBOMB_DAMAGE = 1ULL << 46, FLAG_DONT_SPAWN_IN_CARGEN = 1ULL << 47, FLAG_IS_OFFROAD_VEHICLE = 1ULL << 48, FLAG_INCREASE_PED_COMMENTS = 1ULL << 49, FLAG_EXPLODE_ON_CONTACT = 1ULL << 50 };
enum eVehicleColors { COLOR_BLACK = 0, COLOR_GRAPHITE = 1, COLOR_BLACK_STEEL = 2, COLOR_DARK_STEEL = 3, COLOR_SILVER = 4, COLOR_BLUISH_SILVER = 5, COLOR_ROLLED_STEEL = 6, COLOR_SHADOW_SILVER = 7, COLOR_STONE_SILVER = 8, COLOR_MIDNIGHT_SILVER = 9, COLOR_CAST_IRON_SILVER = 10, COLOR_ANTHRACITE_BLACK = 11, COLOR_MATTE_BLACK = 12, COLOR_MATTE_GRAY = 13, COLOR_MATTE_LIGHT_GRAY = 14, COLOR_UNNAMED_15 = 15, COLOR_UNNAMED_16 = 16, COLOR_UNNAMED_17 = 17, COLOR_UNNAMED_18 = 18, COLOR_UNNAMED_19 = 19, COLOR_UNNAMED_20 = 20, COLOR_UNNAMED_21 = 21, COLOR_UNNAMED_22 = 22, COLOR_UNNAMED_23 = 23, COLOR_UNNAMED_24 = 24, COLOR_UNNAMED_25 = 25, COLOR_UNNAMED_26 = 26, COLOR_RED = 27, COLOR_TORINO_RED = 28, COLOR_FORMULA_RED = 29, COLOR_BLAZE_RED = 30, COLOR_GRACE_RED = 31, COLOR_GARNET_RED = 32, COLOR_SUNSET_RED = 33, COLOR_CABERNET_RED = 34, COLOR_CANDY_RED = 35, COLOR_SUNRISE_ORANGE = 36, COLOR_UNNAMED_37 = 37, COLOR_ORANGE = 38, COLOR_MATTE_RED = 39, COLOR_MATTE_DARK_RED = 40, COLOR_MATTE_ORANGE = 41, COLOR_MATTE_YELLOW = 42, COLOR_UNNAMED_43 = 43, COLOR_UNNAMED_44 = 44, COLOR_UNNAMED_45 = 45, COLOR_UNNAMED_46 = 46, COLOR_UNNAMED_47 = 47, COLOR_UNNAMED_48 = 48, COLOR_DARK_GREEN = 49, COLOR_RACING_GREEN = 50, COLOR_SEA_GREEN = 51, COLOR_OLIVE_GREEN = 52, COLOR_BRIGHT_GREEN = 53, COLOR_GASOLINE_GREEN = 54, COLOR_MATTE_LIME_GREEN = 55, COLOR_UNNAMED_56 = 56, COLOR_UNNAMED_57 = 57, COLOR_UNNAMED_58 = 58, COLOR_UNNAMED_59 = 59, COLOR_UNNAMED_60 = 60, COLOR_GALAXY_BLUE = 61, COLOR_DARK_BLUE = 62, COLOR_SAXON_BLUE = 63, COLOR_BLUE = 64, COLOR_MARINER_BLUE = 65, COLOR_HARBOR_BLUE = 66, COLOR_DIAMOND_BLUE = 67, COLOR_SURF_BLUE = 68, COLOR_NAUTICAL_BLUE = 69, COLOR_ULTRA_BLUE = 70, COLOR_SCHAFTER_PURPLE = 71, COLOR_SPINNAKER_PURPLE = 72, COLOR_RACING_BLUE = 73, COLOR_LIGHT_BLUE = 74, COLOR_UNNAMED_75 = 75, COLOR_UNNAMED_76 = 76, COLOR_UNNAMED_77 = 77, COLOR_UNNAMED_78 = 78, COLOR_UNNAMED_79 = 79, COLOR_UNNAMED_80 = 80, COLOR_UNNAMED_81 = 81, COLOR_MATTE_DARK_BLUE = 82, COLOR_MATTE_BLUE = 83, COLOR_MATTE_MIDNIGHT_BLUE = 84, COLOR_UNNAMED_85 = 85, COLOR_UNNAMED_86 = 86, COLOR_UNNAMED_87 = 87, COLOR_YELLOW = 88, COLOR_RACE_YELLOW = 89, COLOR_BRONZE = 90, COLOR_DEW_YELLOW = 91, COLOR_LIME_GREEN = 92, COLOR_UNNAMED_93 = 93, COLOR_FELTZER_BROWN = 94, COLOR_CREEEN_BROWN = 95, COLOR_CHOCOLATE_BROWN = 96, COLOR_MAPLE_BROWN = 97, COLOR_SADDLE_BROWN = 98, COLOR_STRAW_BROWN = 99, COLOR_MOSS_BROWN = 100, COLOR_BISON_BROWN = 101, COLOR_WOODBEECH_BROWN = 102, COLOR_BEECHWOOD_BROWN = 103, COLOR_SIENNA_BROWN = 104, COLOR_SANDY_BROWN = 105, COLOR_BLEACHED_BROWN = 106, COLOR_CREAM = 107, COLOR_UNNAMED_108 = 108, COLOR_UNNAMED_109 = 109, COLOR_UNNAMED_110 = 110, COLOR_ICE_WHITE = 111, COLOR_FROST_WHITE = 112, COLOR_UNNAMED_113 = 113, COLOR_UNNAMED_114 = 114, COLOR_UNNAMED_115 = 115, COLOR_UNNAMED_116 = 116, COLOR_BRUSHED_STEEL = 117, COLOR_BRUSHED_BLACK_STEEL = 118, COLOR_BRUSHED_ALUMINUM = 119, COLOR_UNNAMED_120 = 120, COLOR_UNNAMED_121 = 121, COLOR_UNNAMED_122 = 122, COLOR_UNNAMED_123 = 123, COLOR_UNNAMED_124 = 124, COLOR_UNNAMED_125 = 125, COLOR_UNNAMED_126 = 126, COLOR_UNNAMED_127 = 127, COLOR_MATTE_GREEN = 128, COLOR_UNNAMED_129 = 129, COLOR_UNNAMED_130 = 130, COLOR_MATTE_ICE_WHITE = 131, COLOR_UNNAMED_132 = 132, COLOR_UNNAMED_134 = 134, COLOR_HOT_PINK = 135, COLOR_SALMON_PINK = 136, COLOR_PFISTER_PINK = 137, COLOR_BRIGHT_ORANGE = 138, COLOR_UNNAMED_139 = 139, COLOR_UNNAMED_140 = 140, COLOR_MIDNIGHT_BLUE = 141, COLOR_MIDNIGHT_PURPLE = 142, COLOR_WINE_RED = 143, COLOR_UNNAMED_146 = 146, COLOR_BRIGHT_PURPLE = 145, COLOR_CARBON_BLACK = 147, COLOR_SCHAFTER_PURPLE_MATTE = 148, COLOR_MIDNIGHT_PURPLE_MATTE = 149, COLOR_LAVA_RED = 150, COLOR_FROST_GREEN = 151, COLOR_OLIVE_DARB = 152, COLOR_UNNAMED_153 = 153, COLOR_DESERT_TAN = 154, COLOR_DARK_EARTH = 155, COLOR_UNNAMED_156 = 156, COLOR_PURE_GOLD = 158, COLOR_BRUSHED_GOLD = 159 };
enum eExplosionTag : int32_t { DONTCARE = -1, GRENADE, GRENADELAUNCHER, STICKYBOMB, MOLOTOV, ROCKET, TANKSHELL, HI_OCTANE, CAR, PLANE, PETROL_PUMP, BIKE, DIR_STEAM, DIR_FLAME, DIR_WATER_HYDRANT, DIR_GAS_CANISTER, BOAT, SHIP_DESTROY, TRUCK, BULLET, SMOKEGRENADELAUNCHER, SMOKEGRENADE, BZGAS, FLARE, GAS_CANISTER, EXTINGUISHER, _0x988620B8, EXP_TAG_TRAIN, EXP_TAG_BARREL, EXP_TAG_PROPANE, EXP_TAG_BLIMP, EXP_TAG_DIR_FLAME_EXPLODE, EXP_TAG_TANKER, PLANE_ROCKET, EXP_TAG_VEHICLE_BULLET, EXP_TAG_GAS_TANK, EXP_TAG_BIRD_CRAP, EXP_TAG_RAILGUN, EXP_TAG_BLIMP2, EXP_TAG_FIREWORK, EXP_TAG_SNOWBALL, EXP_TAG_PROXMINE, EXP_TAG_VALKYRIE_CANNON, EXP_TAG_AIR_DEFENCE, EXP_TAG_PIPEBOMB, EXP_TAG_VEHICLEMINE, EXP_TAG_EXPLOSIVEAMMO, EXP_TAG_APCSHELL, EXP_TAG_BOMB_CLUSTER, EXP_TAG_BOMB_GAS, EXP_TAG_BOMB_INCENDIARY, EXP_TAG_BOMB_STANDARD, EXP_TAG_TORPEDO, EXP_TAG_TORPEDO_UNDERWATER, EXP_TAG_BOMBUSHKA_CANNON, EXP_TAG_BOMB_CLUSTER_SECONDARY, EXP_TAG_HUNTER_BARRAGE, EXP_TAG_HUNTER_CANNON, EXP_TAG_ROGUE_CANNON, EXP_TAG_MINE_UNDERWATER, EXP_TAG_ORBITAL_CANNON, EXP_TAG_BOMB_STANDARD_WIDE, EXP_TAG_EXPLOSIVEAMMO_SHOTGUN, EXP_TAG_OPPRESSOR2_CANNON, EXP_TAG_MORTAR_KINETIC, EXP_TAG_VEHICLEMINE_KINETIC, EXP_TAG_VEHICLEMINE_EMP, EXP_TAG_VEHICLEMINE_SPIKE, EXP_TAG_VEHICLEMINE_SLICK, EXP_TAG_VEHICLEMINE_TAR, EXP_TAG_SCRIPT_DRONE, EXP_TAG_RAYGUN, EXP_TAG_BURIEDMINE, EXP_TAG_SCRIPT_MISSILE, EXP_TAG_RCTANK_ROCKET, EXP_TAG_BOMB_WATER, EXP_TAG_BOMB_WATER_SECONDARY, _0xF728C4A9, _0xBAEC056F, EXP_TAG_FLASHGRENADE, EXP_TAG_STUNGRENADE, _0x763D3B3B, EXP_TAG_SCRIPT_MISSILE_LARGE, EXP_TAG_SUBMARINE_BIG, EXP_TAG_EMPLAUNCHER_EMP };
enum VisibilityFlags : BYTE { InvisibleFlag1 = 36,InvisibleFlag2 = 0,InvisibleFlag3 = 4,IntersectMissionEntityAndTrain = 2,IntersectPeds1 = 4,IntersectVehicles = 10,IntersectVegetation = 256,FrustumCulling = 512,OcclusionCulling = 1024,DistanceCulling = 2048 };
enum ePedVarComp : uint32_t {COMPONENT_HEAD = 0,COMPONENT_FACE = 1,COMPONENT_HAIR = 2,COMPONENT_TORSO = 3,COMPONENT_LEGS = 4,COMPONENT_FEET = 5,COMPONENT_ACCESSORY = 6,COMPONENT_TASK = 7,COMPONENT_UNKNOWN8 = 8,COMPONENT_UNKNOWN9 = 9};
enum VehicleType : __int32 { VEHICLE_TYPE_NONE = 0xFFFFFFFF,VEHICLE_TYPE_CAR = 0x0,VEHICLE_TYPE_PLANE = 0x1,VEHICLE_TYPE_TRAILER = 0x2,	VEHICLE_TYPE_QUADBIKE = 0x3,	VEHICLE_TYPE_DRAFT = 0x4,	VEHICLE_TYPE_SUBMARINECAR = 0x5,	VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE = 0x6,	VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE = 0x7,	VEHICLE_TYPE_HELI = 0x8,VEHICLE_TYPE_BLIMP = 0x9,VEHICLE_TYPE_AUTOGYRO = 0xA,VEHICLE_TYPE_BIKE = 0xB,VEHICLE_TYPE_BICYCLE = 0xC,VEHICLE_TYPE_BOAT = 0xD,VEHICLE_TYPE_TRAIN = 0xE,VEHICLE_TYPE_SUBMARINE = 0xF,VEHICLE_TYPE_NUM = 0x10};
#pragma endregion


class CPed;
class CVehicle;
class CPlayerInfo;
class CWeaponManager;

namespace Core::SDK::Pointers {
	extern CPed* pLocalPlayer;
}

// Entity is a remote CPhysical address in this SDK, not a native script GUID.
using Entity = uintptr_t;

class NetworkControl
{
	friend class CPed;
	friend class CVehicle;

private:
	struct NativeAddresses {
		uintptr_t ModuleBase = 0;
		uintptr_t ModuleSize = 0;
		DWORD ProcessId = 0;
		uint32_t Build = 0;
		uintptr_t Request = 0;
		uintptr_t Has = 0;
	};

	// Nested entity operations share one request per target; standalone calls
	// still make one attempt. This scope does not wait for ownership.
	static inline thread_local unsigned OperationDepth = 0;
	static inline thread_local std::vector<Entity> RequestedEntities;
	class Operation {
	public:
		Operation() { if (OperationDepth++ == 0) RequestedEntities.clear(); }
		explicit Operation(const std::vector<Entity>& InheritedRequests) {
			if (OperationDepth++ == 0) RequestedEntities = InheritedRequests;
		}
		~Operation() { if (--OperationDepth == 0) RequestedEntities.clear(); }
		Operation(const Operation&) = delete;
		Operation& operator=(const Operation&) = delete;
	};

	static uintptr_t FindUniqueNative(const std::string& Pattern)
	{
		const uintptr_t Address = Mem.FindSignatureStr(Pattern);
		if (!Address || Address < Mem.ModBase || !Mem.ModBaseSize ||
			Mem.ModBaseSize > UINTPTR_MAX - Mem.ModBase)
			return 0;
		const uintptr_t End = Mem.ModBase + Mem.ModBaseSize;
		if (Address >= End || (Address + 1 < End &&
			Mem.FindSignatureStr(Pattern, Address + 1, End - Address - 1)))
			return 0;
		// The existing scanner uses zero bytes as wildcards. Check literal
		// zeroes too before accepting a native implementation.
		const size_t Length = Mem.Pattern2Vector(Pattern).size();
		if (Length > End - Address)
			return 0;
		std::vector<uint8_t> Bytes(Length);
		if (!Mem.ReadBuffer(Address, Bytes.data(), Bytes.size()))
			return 0;
		std::istringstream Tokens(Pattern);
		std::string Token;
		size_t Index = 0;
		while (Tokens >> Token) {
			if (Token != xorstr("?") && Token != xorstr("??") &&
				Bytes[Index] != static_cast<uint8_t>(std::stoul(Token, nullptr, 16)))
				return 0;
			++Index;
		}
		return Address;
	}

	static NativeAddresses ResolveNatives()
	{
		static std::mutex CacheMutex;
		static NativeAddresses Cache;
		std::lock_guard<std::mutex> Lock(CacheMutex);
		if (!Mem.ProcHandle || !Mem.ModBase || !Mem.ModBaseSize)
			return {};
		if (Cache.ModuleBase == Mem.ModBase && Cache.ModuleSize == Mem.ModBaseSize &&
			Cache.ProcessId == Mem.ProcId && Cache.Build == g_Offsets.CurrentBuild)
			return Cache;

		NativeAddresses Natives;
		Natives.ModuleBase = Mem.ModBase;
		Natives.ModuleSize = Mem.ModBaseSize;
		Natives.ProcessId = Mem.ProcId;
		Natives.Build = g_Offsets.CurrentBuild;
		// Verified in the open IDB: network_commands::CommandNetwork*OfEntity(int).
		// Match the implementation's body as well as its prologue; never use a
		// fixed RVA or the lower-level CRequestControlEvent::Trigger directly.
		Natives.Request = FindUniqueNative(xorstr(
			"48 89 5C 24 ? 57 48 83 EC 20 8B D9 E8 ? ? ? ? 84 C0 75 ? "
			"8B CB E8 ? ? ? ? 48 8B F8 48 85 C0 74 ? "
			"48 8B 98 ? ? ? ? 48 85 DB 74 ? 80 7B ? 00 "
			"75 ? 48 8B CB E8 ? ? ? ? 84 C0 74"
		));
		Natives.Has = FindUniqueNative(xorstr(
			"48 83 EC 28 E8 ? ? ? ? 48 85 C0 74 ? "
			"48 8B 88 ? ? ? ? 48 85 C9 74 ? 80 79 ? 00 75 ? "
			"E8 ? ? ? ? 84 C0 75 ? B0 01 EB ? 32 C0 48 83 C4 28 C3"
		));
		Cache = Natives;
		return Cache;
	}

	static bool CallNative(Entity entity, bool Request)
	{
		if (!g_Offsets.m_GetScriptGuidFromBase)
			return false;
		const NativeAddresses Natives = ResolveNatives();
		const uintptr_t Native = Request ? Natives.Request : Natives.Has;
		if (!Native)
			return false;

		// x64 ABI: CPhysical* -> int script GUID -> native(int), result in AL.
		// No ownership retry, polling, delay or asynchronous worker is added.
		std::array<uint8_t, 56> Code = {
			0x48, 0x83, 0xEC, 0x28,                         // sub rsp, 28h
			0x48, 0xB9, 0, 0, 0, 0, 0, 0, 0, 0,             // mov rcx, entity
			0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,             // mov rax, ToGuid
			0xFF, 0xD0, 0x85, 0xC0, 0x74, 0x16,             // call; test eax; jz failure
			0x8B, 0xC8,                                     // mov ecx, eax
			0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,             // mov rax, native
			0xFF, 0xD0, 0x0F, 0xB6, 0xC0,                   // call; movzx eax, al
			0x48, 0x83, 0xC4, 0x28, 0xC3,                   // restore stack; ret
			0x33, 0xC0, 0xEB, 0xF7                          // failure: xor eax; jmp restore
		};
		std::memcpy(Code.data() + 6, &entity, sizeof(entity));
		std::memcpy(Code.data() + 16, &g_Offsets.m_GetScriptGuidFromBase, sizeof(uintptr_t));
		std::memcpy(Code.data() + 34, &Native, sizeof(Native));
		const uintptr_t Cave = Mem.CreateDataCave(Code.size());
		if (!Cave)
			return false;
		const DWORD Result = Mem.execShellcode(Cave, Code.data(), Code.size());
		Mem.FreeCave(Cave, Code.size());
		return Result == 1;
	}

public:
	static inline void RequestControl(Entity entity)
	{
		if (!entity)
			return;
		if (OperationDepth) {
			if (std::find(RequestedEntities.begin(), RequestedEntities.end(), entity) != RequestedEntities.end())
				return;
			RequestedEntities.push_back(entity);
		}
		CallNative(entity, true);
	}

	static inline bool HasControl(Entity entity)
	{
		if (!entity)
			return false;
		return CallNative(entity, false);
	}
};

#pragma region Infos
class CPlayerInfo {
public:
	int PlayerID( ) {
		if (!this || !g_Offsets.m_PlayerId) { return -1; }
		return Mem.Read<int>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_PlayerId);
	}
};

class CWeaponInfo {
public:
	std::string GetName() {
		if (!this) { return xorstr(""); }
		return Mem.ReadString(Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x5F0));
	}
};
#pragma endregion

#pragma region List
class CVehicleList {
public:
	CVehicle * Vehicle( int Idx )
	{
		if ( !this ) { return 0; }
		return ( CVehicle * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + ( Idx * 0x10U ) );
	}

};

class CPedList {
public:
	CPed * Ped( int Idx ) {
		if ( !this ) { return 0; }
		return ( CPed * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + ( Idx * 0x10U ) );
	}
};
#pragma endregion

#pragma region InterFaces
class CPedInterFace {
public:
	int MaxPed( ) { return Mem.Read<int>( reinterpret_cast< uintptr_t >( this ) + 0x108 ); }
	int PedCount( ) { return Mem.Read<int>( reinterpret_cast< uintptr_t >( this ) + 0x110 ); }
	CPedList * PedList( ) { return ( CPedList * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x100 ); }
};

class CVehInterFace {
public:
	int MaxVehicles( ) { return Mem.Read<int>( reinterpret_cast< uintptr_t >( this ) + 0x188 ); }
	int VehicleCount( ) { return Mem.Read<int>( reinterpret_cast< uintptr_t >( this ) + 0x190 ); }
	CVehicleList * VehicleList( ) { return ( CVehicleList * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x180 ); }
};

class CReplayInterFace {
public:

	CPedInterFace * InterfacePed( ) {
		if ( !this ) { return 0; }
		return ( CPedInterFace * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x18 );
	}

	CVehInterFace * InterfaceVeh( ) {
		if ( !this ) { return 0; }
		return ( CVehInterFace * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x10 );
	}
};

#pragma endregion

class CamFollowPedCamera
{
public:
	D3DXVECTOR3 GetViewAngles()
	{
		if (!this)
			return D3DXVECTOR3(0, 0, 0);

		return Mem.Read<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x40);
	}

	void SetViewAngles(D3DXVECTOR3 Angles)
	{
		if (!this)
			return;

		Mem.Write(reinterpret_cast<uintptr_t>(this) + 0x40, Angles);
	}

	D3DXVECTOR3 GetCrosshairPosition()
	{
		if (!this)
			return D3DXVECTOR3(0, 0, 0);

		return Mem.Read<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x60);
	}

	void SetCrosshairPosition(D3DXVECTOR3 Position)
	{
		if (!this)
			return;

		Mem.Write(reinterpret_cast<uintptr_t>(this) + 0x60, Position);
	}

	D3DXVECTOR3 GetThirdpersonViewAngles()
	{
		if (!this)
			return D3DXVECTOR3(0, 0, 0);

		return Mem.Read<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x3d0);
	}

	void SetThirdpersonViewAngles(D3DXVECTOR3 ViewAngles)
	{
		if (!this)
			return;

		Mem.Write(reinterpret_cast<uintptr_t>(this) + 0x3d0, ViewAngles);
	}


	D3DXVECTOR3 GetCameraPosition()
	{
		if (!this)
			return D3DXVECTOR3(0, 0, 0);

		return Mem.Read<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x3F0);
	}
};

class CamGameplayDirector
{
public:
	CamFollowPedCamera* GetFollowPedCamera()
	{
		if (!this)
			return 0;

		return (CamFollowPedCamera*)Mem.Read<uint64_t>(reinterpret_cast<uintptr_t>(this) + 0x3C0);
	}
};


class CPed {
public:

	float GetMaxHealth() {
		if (!this) { return 0; }
		return Mem.Read<float>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_MaxHealth);
	}

	CPed* pNetObject() {
		if (!this) return 0;
		return (CPed*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0xD0);
	}

	uint8_t mOwnerId() {
		return Mem.Read<uint8_t>((uintptr_t)this + 0x49);
	}

	void fClearTask(float fValue) {
		if (!this) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);

		Core::Mem.Write<float>((uintptr_t)this + 0x2C, fValue);
	}

	void vSetLocation(D3DXVECTOR3 vLocation) {
		if (!this) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);

		Core::Mem.Write<D3DXVECTOR3>((uintptr_t)this + 0x50, vLocation);
	}

	D3DXVECTOR3 vLocation() {
		return Core::Mem.Read<D3DXVECTOR3>((uintptr_t)this + 0x50);
	}

	D3DXVECTOR3 vRotation(bool bIsFristPerson) {
		if (bIsFristPerson) {
			return Core::Mem.Read<D3DXVECTOR3>((uintptr_t)this + 0x40);
		}
		else {
			return Core::Mem.Read<D3DXVECTOR3>((uintptr_t)this + 0x3D0);
		}
	}

	CamGameplayDirector* CamGamePlayDirector()
	{
		return reinterpret_cast<CamGameplayDirector*>( Mem.Read<uintptr_t>(g_Offsets.m_CamGameplayDirector) );
	}

	void SetInvisible(bool state)
	{
		if (!this) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);

		uintptr_t pedAddr = reinterpret_cast<uintptr_t>(this);

		uint64_t netObj = Mem.Read<uint64_t>(pedAddr + 0xD0);
		if (netObj) {
			Mem.Write<uint8_t>(netObj + 0xD1, state ? 0x0D : 0x00);

			uint8_t bitfield = Mem.Read<uint8_t>(netObj + 0xD6);
			if (state)
				bitfield &= ~0x02;
			else
				bitfield |= 0x02;
			Mem.Write<uint8_t>(netObj + 0xD6, bitfield);
		}
	}

	uintptr_t GetPlayerModel()
	{
		if (!this)
			return 0;

		uintptr_t v1 = Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x50);
		if (!v1) return 0;

		uintptr_t v2 = (g_Offsets.CurrentBuild >= 2189)
			? Mem.Read<uintptr_t>(v1 + 0x20)
			: Mem.Read<uintptr_t>(v1 + 0x40);
		if (!v2) return 0;

		uintptr_t v3 = (g_Offsets.CurrentBuild >= 2189)
			? Mem.Read<uintptr_t>(v2 + 0x0)
			: Mem.Read<uintptr_t>(v2 + 0x8);
		if (!v3) return 0;

		uintptr_t v4 = Mem.Read<uintptr_t>(v3 + 0x20);
		if (!v4) return 0;

		return v4 + 0x18;
	}

	uint64_t GetModelInfo()
	{
		if (!this)	return 0;
		return Mem.Read<uint64_t>(reinterpret_cast<uint64_t>(this) + 0x20);
	}

	void ChangeModel(bool& toggle)
	{
		if (!this) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		uintptr_t modelAddr = GetPlayerModel();
		if (!modelAddr) return;

		if (toggle)
		{
			Mem.Write<uint32_t>(modelAddr, 3283429734);
			std::this_thread::sleep_for(std::chrono::seconds(1));
			Mem.Write<uint32_t>(modelAddr, 2627665880);
			toggle = false;
		}
	}

	bool GetIsInvisible()
	{
		if (!this) return false;

		uint64_t netObj = Mem.Read<uint64_t>((uintptr_t)this + 0xD0);
		if (!netObj) return false;

		return Mem.Read<uint8_t>(netObj + 0xD1) != 0;
	}

	float GetHealth() {
		if (!this) { return 0; }
		return Mem.Read<float>(reinterpret_cast<uintptr_t>(this) + 0x280);
	}

	void SetHealth(float Health) {
		if (!this) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + 0x280, Health);
	}

	float GetArmor() {
		if (!this) { return 0; }
		return Mem.Read<float>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_Armor);
	}

	void SetArmor(float Armor) {
		if (!this) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_Armor, Armor);
	}

	float GetSpeed() {
		if (!this) { return 0; }
		CPlayerInfo* PlayerInfo = this->GetPlayerInfo();
		if (!PlayerInfo) { return 0; }
		return Mem.Read<float>(reinterpret_cast<uintptr_t>(PlayerInfo) + g_Offsets.m_Speed);
	}

	void SetSpeed(float Speed) {
		if (!this) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		CPlayerInfo* PlayerInfo = this->GetPlayerInfo();
		if (!PlayerInfo) { return; }
		Mem.Write<float>(reinterpret_cast<uintptr_t>(PlayerInfo) + g_Offsets.m_Speed, Speed);
	}

	uintptr_t GetFxDrawData() {
		if (!this) { return 0; }
		return Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x48);
	}

	void NoCollision(bool toggle)
	{
		if (!this) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		uint64_t v1 = Mem.Read<uint64_t>(this->GetNavigation() + 0x10);
		uint64_t v2 = Mem.Read<uint64_t>(v1 + 0x20);
		uint64_t v3 = Mem.Read<uint64_t>(v2 + 0x70);
		uint64_t v4 = Mem.Read<uint64_t>(v3);
		Mem.Write(v4 + 0x2C, toggle ? -0.9999f : 0.25f);
	}

	void SeatBealt(bool toggle) {
		if (!this) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		bool InsideVehicle = InVehicle();

		if (InsideVehicle) {
			if (toggle) {
				uintptr_t Path = Mem.FindSignature({ 0x83, 0xa1, 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x83, 0xe2 });
				Mem.WriteBytes(Path, { 0x90,0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, });
				Mem.Write<BYTE>((uintptr_t)this + g_Offsets.m_SeatBealt, 0xC9);
			}
			else {
				Mem.Write<BYTE>((uintptr_t)this + g_Offsets.m_SeatBealt, 0xC8);
			}
		}

	}

	CVehicle* GetLastVehicle() {
		if (!this || !g_Offsets.m_LastVehicle) return nullptr;
		return (CVehicle*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_LastVehicle);
	}

	CPlayerInfo* GetPlayerInfo() {
		if (!this || !g_Offsets.m_PlayerInfo) return nullptr;
		return (CPlayerInfo*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_PlayerInfo);
	}

	CWeaponManager* GetWeaponManager() {
		if (!this || !g_Offsets.m_WeaponManager) return nullptr;
		return (CWeaponManager*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_WeaponManager);
	}

	uint32_t GetPedType() {
		if (!this) { return 0; }
		return Mem.Read<uint32_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_EntityType) << 11 >> 25;
	}

	bool IsNPC() {
		return GetPedType() != 2;
	}

	bool IsAnimal() {
		return GetPedType() > 27;
	}

	int GetID() {
		if (!this) { return 0; }
		CPlayerInfo* PlayerInfo = (CPlayerInfo*)GetPlayerInfo();
		if (!PlayerInfo) { return 0; }
		int Id = PlayerInfo->PlayerID();
		return Id;
	}

	bool HasFlag(ePedConfigFlag Flag)
	{
		if (!this || !g_Offsets.m_PedFlag) { return false; }

		auto v1 = (int)Flag;
		if (!this || v1 < 0 || v1 > 0x1CA) return false;

		const uint32_t v2 = 1u << (v1 & 0x1F);
		auto v3 = v1 >> 5;
		auto v4 = reinterpret_cast<uintptr_t>(this) + 4 * v3 + g_Offsets.m_PedFlag;
		auto v5 = Mem.Read<long>(v4);

		return (v2 & v5) != 0;
	}

	void SetConfigFlag(ePedConfigFlag Flag, bool Value)
	{
		if (!this || !g_Offsets.m_PedFlag) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);

		auto v1 = (int)Flag;
		if (!this || v1 < 0 || v1 > 0x1CA) return;

		const uint32_t v2 = 1u << (v1 & 0x1F);
		auto v3 = v1 >> 5;
		auto v4 = (uintptr_t)(this) + 4 * v3 + g_Offsets.m_PedFlag;
		auto v5 = Mem.Read<long>(v4);

		if (Value != ((v2 & v5) != 0)) {
			auto v6 = v2 & (v5 ^ -(uint8_t)(Value ? 1 : 0));
			v5 ^= v6;
			Mem.Write(v4, v5);
		}
	}

	bool InVehicle( ) {
		if ( !this ) return false;
		return HasFlag( ePedConfigFlag::InVehicle );
	}

	bool IsShooting() {
		if (!this) return false;
		return HasFlag(ePedConfigFlag::IsShooting);
	}

	void NoRagDoll( bool Toggle )
	{
		if ( !this ) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		Mem.Write<BYTE>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_NoRagDoll, Toggle ? 0x80 : 0x20 );
	}

	D3DXVECTOR3 GetVelocity( ) {
		if ( !this ) { return D3DXVECTOR3( 0, 0, 0 ); }
		return Mem.Read<D3DXVECTOR3>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_Speed );
	}

	void FreezePed( bool Toggle ) {
		if ( !this ) { return; }
		NetworkControl::Operation ControlOperation;
		if ( !InVehicle( ) ) {
			const Entity entity = reinterpret_cast<uintptr_t>(this);
			if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
				NetworkControl::RequestControl(entity);
			uintptr_t CModelInfo = Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x20 );
			Mem.Write<float>( CModelInfo + 0x2C, Toggle ? 0.f : 1.f );
		}
		else {
			CVehicle* veh = GetLastVehicle();
			if ( !veh ) { return; }
			const Entity entity = reinterpret_cast<uintptr_t>(veh);
			if (!NetworkControl::HasControl(entity))
				NetworkControl::RequestControl(entity);
			Mem.Write<BYTE>( reinterpret_cast< uintptr_t >( veh ) + 0x2E, Toggle ? 1 : 2 );
		}
	}

	void SetBlockAimming(bool Toggle){
		if (!this) return;
		SetConfigFlag(DisablePlayerLockon, Toggle);
	}

	void SetGodMode(bool Toggle)
	{
		static std::mutex GodModeMutex;
		std::lock_guard<std::mutex> lock(GodModeMutex);
		const uintptr_t Ped = reinterpret_cast<uintptr_t>(this);
		const DWORD Pid = Mem.ProcId;
		if (!Ped || !Pid || !Mem.ProcHandle)
		{ return; }
		if (!g_Offsets.m_PlayerInfo || !g_Offsets.m_Godmod
			|| g_Offsets.m_InvincibilityFlags < 0x100 || g_Offsets.m_InvincibilityFlags >= 0x1000)
		{ return; }
		uintptr_t PI = 0;
		uint32_t Flags = 0;
		if (!Mem.ReadBuffer(Ped + g_Offsets.m_PlayerInfo, &PI, sizeof(PI)) || !PI
			|| !Mem.ReadBuffer(Ped + g_Offsets.m_InvincibilityFlags, &Flags, sizeof(Flags)))
		{ return; }

		// CPhysical invincibility bit 8 covers damage outside the weapon-defense path.
		// Preserve unrelated flags, including bit 9; change only the bit this method owns.
		constexpr uint32_t InvincibleBit = 1u << 8;
		const uint32_t DesiredFlags = Toggle ? Flags | InvincibleBit : Flags & ~InvincibleBit;
		if (Flags != DesiredFlags && !Mem.Write<uint32_t>(Ped + g_Offsets.m_InvincibilityFlags, DesiredFlags))
		{ return; }

		const float Value = Toggle ? 0.f : 1.f;
		const uintptr_t Address = PI + g_Offsets.m_Godmod;
		float Current = 0.f;
		if (!Mem.ReadBuffer(Address, &Current, sizeof(Current)))
		{ return; }
		if (Current != Value)
		{
			if (!Mem.Write<float>(Address, Value))
				{ return; }
			if (!Mem.ReadBuffer(Address, &Current, sizeof(Current)) || Current != Value)
				{ return; }
		}
		uint32_t VerifiedFlags = 0;
		if (!Mem.ReadBuffer(Ped + g_Offsets.m_InvincibilityFlags, &VerifiedFlags, sizeof(VerifiedFlags))
			|| (VerifiedFlags & InvincibleBit) != (DesiredFlags & InvincibleBit))
		{ return; }
		SetConfigFlag(ePedConfigFlag::DontActivateRagdollFromBulletImpact, Toggle);
		SetConfigFlag(ePedConfigFlag::DontActivateRagdollFromExplosions, Toggle);
	}

	void SetInfStamina(bool Toggle)
	{
		if (!this) { return; }
		CPlayerInfo* PlayerInfo = (CPlayerInfo*)GetPlayerInfo();
		if (!PlayerInfo) { return; }
		uintptr_t playerInfoAddr = reinterpret_cast<uintptr_t>(PlayerInfo);
		if (Toggle) {
			float maxStamina = Mem.Read<float>(playerInfoAddr + g_Offsets.m_MaxStamina);
			Mem.Write<float>(playerInfoAddr + g_Offsets.m_Stamina, maxStamina);
		}
	}

	bool IsVisible() {
		if (!this) return false;

		uintptr_t pointer = reinterpret_cast<uintptr_t>(this);

		if (g_Offsets.m_IsEntityVisible != 0) {
			uint32_t original_flags = Mem.Read<uint32_t>(pointer + 0xC0);

			if (!(original_flags & (1 << 29))) {
				uint32_t modified_flags = original_flags | (1 << 29);
				Mem.Write<uint32_t>(pointer + 0xC0, modified_flags);
			}

			uint64_t fx_draw_data = Mem.Read<uint64_t>(pointer + 0x48);
			uint32_t query_id = static_cast<uint32_t>(Mem.Read<uint8_t>(fx_draw_data + 0x2B)) & 0xFF;
			uintptr_t base_offset = Mem.ModBase + g_Offsets.m_IsEntityVisible;

			return Mem.Read<int>(base_offset + ((query_id - 1) * 0x80) + 0x78) > 100;
		}
		else {
			float LastVisibleTime = Mem.Read<float>(pointer + 0x1A0);
			return LastVisibleTime > 0.1f;
		}
	}

	void SetInfCombatRoll( bool enable ) {
		if ( !this ) { return; }

		uintptr_t Address = g_Offsets.m_InfiniteCombatRoll;
		static std::vector<uint8_t> OriginalTable;

		if ( OriginalTable.empty( ) ) {
			OriginalTable = Mem.ReadBytes( Address, 6 );
		}

		std::vector <uint8_t> Patch = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

		Mem.WriteBytes( Address, enable ? Patch : OriginalTable );
	}

	D3DXVECTOR3 GetBonePosDefault( const int Bone )
	{
		if ( !this ) { return D3DXVECTOR3( 0, 0, 0 ); }
		D3DXMATRIX Mtx = Mem.Read<D3DXMATRIX>( reinterpret_cast< uintptr_t >( this ) + 0x60 );
		// O ternario estava sem parenteses: em build >= 2802 o "+ Bone * 0x10"
		// pertencia ao ramo do 0x430 e nao era aplicado, entao TODO osso pedido
		// devolvia o mesmo ponto -- o primeiro do array. So o osso 0 (cabeca)
		// dava certo por acidente, e era por isso que a caixa por osso
		// colapsava num quadradinho na cabeca.
		const uintptr_t BoneArray = ( g_Offsets.CurrentBuild >= 2802 ) ? 0x410 : 0x430;
		D3DXVECTOR3 BonePos = Mem.Read<D3DXVECTOR3>( reinterpret_cast< uintptr_t >( this ) + BoneArray + Bone * 0x10 );

		D3DXVECTOR4 Transform;
		D3DXVec3Transform( &Transform, &BonePos, &Mtx );
		return D3DXVECTOR3( Transform.x, Transform.y, Transform.z );
	}

	float GetDistance( D3DXVECTOR3 pos1, D3DXVECTOR3 pos2 ) {
		return std::sqrtf( std::pow( pos2.x - pos1.x, 2.f ) + std::pow( pos2.y - pos1.y, 2.f ) );
	}

	uintptr_t GetCPedInventory( ) {
		if ( !this ) { return 0; }
		return Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + ( g_Offsets.m_WeaponManager - 8 ) );
	}

	uintptr_t GetNavigation( ) {
		if ( !this ) { return 0; }
		return Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x30 );
	}

	void RemoveKinematics( )
	{
		if ( !g_Offsets.m_ArmsKinematics || !g_Offsets.m_LegsKinematics )
			return;

		Mem.PatchFunc( g_Offsets.m_ArmsKinematics, 5 );
		Mem.PatchFunc( g_Offsets.m_LegsKinematics, 5 );
	}

	void ForceWeaponWheel( bool toggle )
	{
		if ( !this ) return;
		static uintptr_t DisableControlAction = 0;

		if ( DisableControlAction == 0 ) {
			DisableControlAction = Mem.FindSignature( 
				{ 0x48, 0x89, 0x5C, 0x24, 0x00, 0x48, 0x89, 0x6C, 0x24, 0x00, 0x48, 0x89, 0x74, 0x24, 0x00, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x41, 0x8A, 0xF0, 0x8B, 0xDA, 0x8B, 0xF9 }
			);
		}

		if (toggle)
		{
			Mem.Write<uint64_t>(DisableControlAction, 0x6C894890909090C3ULL);
			SetConfigFlag(BlockWeaponSwitching, false);
		}
		else
		{
			Mem.Write<uint64_t>(DisableControlAction, 0x6C894808245C8948ULL);
			SetConfigFlag(BlockWeaponSwitching, true);
		}
	}

	void ThermalVision(bool toggle)
	{
		if (!this) return;

		if (toggle) {
			Mem.Write<uintptr_t>(g_Offsets.m_ThermalVision, 1);
		}
		else {
			Mem.Write<uintptr_t>(g_Offsets.m_ThermalVision, 0);
		}
	}

	uint8_t GetOwnerId() const {
		return Core::Mem.Read<uint8_t>((uintptr_t)this + 0x49);
	}

	void SetOwnerId(uint8_t ownerId) {
		Core::Mem.Write<uint8_t>((uintptr_t)this + 0x49, ownerId);
	}

	void NetworkRequestControl(CVehicle* vehicle)
	{
		if (!this || !vehicle) return;
		const Entity entity = reinterpret_cast<uintptr_t>(vehicle);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
	}

	bool SetInVehicle(uintptr_t vehicle, int seat, int flags = 0)
	{
		if (!this || !vehicle || !g_Offsets.m_SetPedInVehicleFunc)
			return false;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (this != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		if (!NetworkControl::HasControl(vehicle))
			NetworkControl::RequestControl(vehicle);

		uintptr_t ped = reinterpret_cast<uintptr_t>(this);
		uintptr_t funcAddr = g_Offsets.m_SetPedInVehicleFunc;

		auto push8 = [](std::vector<uint8_t>& v, uint8_t b) { v.push_back(b); };
		auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
			for (int i = 0; i < 4; i++) v.push_back((val >> (i * 8)) & 0xFF);
		};
		auto push64 = [](std::vector<uint8_t>& v, uint64_t val) {
			for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
		};
		auto pushJunk = [&](std::vector<uint8_t>& v) {
			int n = 1 + (std::rand() % 3);
			for (int j = 0; j < n; j++) {
				switch (std::rand() % 8) {
				case 0: push8(v, 0x90); break;
				case 1: push8(v, 0x48); push8(v, 0x87); push8(v, 0xC0); break;
				case 2: push8(v, 0x4D); push8(v, 0x85); push8(v, 0xD2); break;
				case 3: push8(v, 0x49); push8(v, 0x83); push8(v, 0xCA); push8(v, 0x00); break;
				case 4: push8(v, 0x66); push8(v, 0x90); break;
				case 5: push8(v, 0x0F); push8(v, 0x1F); push8(v, 0x00); break;
				case 6: push8(v, 0x49); push8(v, 0xBA); push64(v, (uint64_t)std::rand()); break;
				case 7: push8(v, 0x49); push8(v, 0xBB); push64(v, (uint64_t)std::rand()); break;
				}
			}
		};

		std::vector<uint8_t> sc;
		sc.reserve(0x100);

		pushJunk(sc);
		// sub rsp, 0x28
		push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xEC); push8(sc, 0x28);
		pushJunk(sc);
		// mov rcx, ped
		push8(sc, 0x48); push8(sc, 0xB9); push64(sc, ped);
		pushJunk(sc);
		// mov rdx, vehicle
		push8(sc, 0x48); push8(sc, 0xBA); push64(sc, vehicle);
		pushJunk(sc);
		// mov r8d, seat
		push8(sc, 0x41); push8(sc, 0xB8); push32(sc, (uint32_t)seat);
		pushJunk(sc);
		// mov r9d, flags
		if (flags == 0) {
			push8(sc, 0x45); push8(sc, 0x31); push8(sc, 0xC9);
		} else {
			push8(sc, 0x41); push8(sc, 0xB9); push32(sc, (uint32_t)flags);
		}
		pushJunk(sc);
		// mov rax, funcAddr
		push8(sc, 0x48); push8(sc, 0xB8); push64(sc, funcAddr);
		// call rax
		push8(sc, 0xFF); push8(sc, 0xD0);
		// add rsp, 0x28
		push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xC4); push8(sc, 0x28);
		// ret
		push8(sc, 0xC3);

		uintptr_t cave = Core::Mem.CreateDataCave(0x200);
		if (!cave) return false;

		DWORD res = Mem.execShellcode(cave, sc.data(), sc.size());
		Core::Mem.FreeCave(cave, sc.size());
		return res != 0;
	}

	D3DXVECTOR3 GetPos() {
		if (!this) { return D3DXVECTOR3(0, 0, 0); }
		return Mem.Read<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x90);
	}

	void SetPos(D3DXVECTOR3 Pos)
	{
		if (!this) return;
		bool InsideVehicle = InVehicle();
		uintptr_t LastVeh = reinterpret_cast<uintptr_t>(GetLastVehicle());
		if (LastVeh && InsideVehicle)
		{
			if (!NetworkControl::HasControl(LastVeh))
				NetworkControl::RequestControl(LastVeh);
			uintptr_t Navigation = Mem.Read<uintptr_t>(LastVeh + 0x30);

			Mem.Write<D3DXVECTOR3>(Navigation + 0x30, D3DXVECTOR3(0, 0, 0));
			Mem.Write<D3DXVECTOR3>(Navigation + 0x50, Pos);
			Mem.Write<D3DXVECTOR3>(LastVeh + 0x90, Pos);
		}
		else
		{
			uintptr_t Navigation = GetNavigation();
			Mem.Write<D3DXVECTOR3>(Navigation + 0x30, D3DXVECTOR3(0, 0, 0));
			Mem.Write<D3DXVECTOR3>(Navigation + 0x50, Pos);
			Mem.Write<D3DXVECTOR3>(reinterpret_cast<uintptr_t>(this) + 0x90, Pos);
		}
	}

	void SpectatePed(uint64_t Ped, bool Toggle)
	{
		static std::vector<uint8_t> OriginalCamBytes;
		static std::vector<uint8_t> NopTable(7, 0x90);

		if (OriginalCamBytes.empty())
		{
			OriginalCamBytes.resize(NopTable.size());
			for (size_t i = 0; i < NopTable.size(); i++)
				OriginalCamBytes[i] = Mem.Read<BYTE>(g_Offsets.m_GameplayCamHolder + i);
		}

		if (Toggle)
			Mem.WriteBytes(g_Offsets.m_GameplayCamHolder, NopTable);
		else
			Mem.WriteBytes(g_Offsets.m_GameplayCamHolder, OriginalCamBytes);

		Mem.Write<uint64_t>(g_Offsets.m_GameplayCamTarget, Toggle ? Ped : 0x0);
	}	
};

class CHandlingData {
public:

	uint64_t qword0; //0x0000
	uint32_t m_model_hash; //0x0008
	float m_mass; //0x000C
	float m_initial_drag_coeff; //0x0010
	float m_downforce_multiplier; //0x0014
	float m_popup_light_rotation; //0x0018
	char pad_001C[ 4 ]; //0x001C
	D3DXVECTOR3 m_centre_of_mass; //0x0020
	char pad_002C[ 4 ]; //0x002C
	D3DXVECTOR3 m_inertia_mult; //0x0030
	char pad_003C[ 4 ]; //0x003C
	float m_buoyancy; //0x0040
	float m_drive_bias_rear; //0x0044
	float m_drive_bias_front; //0x0048
	float m_acceleration; //0x004C
	uint8_t m_initial_drive_gears; //0x0050
	char pad_0051[ 3 ]; //0x0051
	float m_drive_inertia; //0x0054
	float m_upshift; //0x0058
	float m_downshift; //0x005C
	float m_initial_drive_force; //0x0060
	float m_drive_max_flat_velocity; //0x0064
	float m_initial_drive_max_flat_vel; //0x0068
	float m_brake_force; //0x006C
	char pad_0070[ 4 ]; //0x0070
	float m_brake_bias_front; //0x0074
	float m_brake_bias_rear; //0x0078
	float m_handbrake_force; //0x007C
	float m_steering_lock; //0x0080
	float m_steering_lock_ratio; //0x0084
	float m_traction_curve_max; //0x0088
	float m_traction_curve_lateral; //0x008C
	float m_traction_curve_min; //0x0090
	float m_traction_curve_ratio; //0x0094
	float m_curve_lateral; //0x0098
	float m_curve_lateral_ratio; //0x009C
	float m_traction_spring_delta_max; //0x00A0
	float m_traction_spring_delta_max_ratio; //0x00A4
	float m_low_speed_traction_loss_mult; //0x00A8
	float m_camber_stiffness; //0x00AC
	float m_traction_bias_front; //0x00B0
	float m_traction_bias_rear; //0x00B4
	float m_traction_loss_mult; //0x00B8
	float m_suspension_force; //0x00BC
	float m_suspension_comp_damp; //0x00C0
	float m_suspension_rebound_damp; //0x00C4
	float m_suspension_upper_limit; //0x00C8
	float m_suspension_lower_limit; //0x00CC
	float m_suspension_raise; //0x00D0
	float m_suspension_bias_front; //0x00D4
	float m_suspension_bias_rear; //0x00D8
	float m_anti_rollbar_force; //0x00DC
	float m_anti_rollbar_bias_front; //0x00E0
	float m_anti_rollbar_bias_rear; //0x00E4
	float m_roll_centre_height_front; //0x00E8
	float m_roll_centre_height_rear; //0x00EC
	float m_collision_damage_mult; //0x00F0
	float m_weapon_damamge_mult; //0x00F4
	float m_deformation_mult; //0x00F8
	float m_engine_damage_mult; //0x00FC
	float m_petrol_tank_volume; //0x0100
	float m_oil_volume; //0x0104
	char pad_0108[ 4 ]; //0x0108
	D3DXVECTOR3 m_seat_offset_dist; //0x010C
	uint32_t m_monetary_value; //0x0118
	char pad_011C[ 8 ]; //0x011C
	uint32_t m_model_flags; //0x0124
	uint32_t m_handling_flags; //0x0128
	uint32_t m_damage_flags; //0x012C
	char pad_0130[ 12 ]; //0x0130
	uint32_t m_ai_handling_hash; //0x013C
	char pad_140[ 24 ]; //0x140
};

class CVehicle {
public:
	struct RocketBoostNatives {
		uintptr_t ModuleBase = 0;
		uint32_t Build = 0;
		uintptr_t SetPercentage = 0;
		uintptr_t SetRefillTime = 0;
	};

private:
	static RocketBoostNatives GetRocketBoostNatives()
	{
		static std::mutex CacheMutex;
		static RocketBoostNatives Cache;

		std::lock_guard<std::mutex> Lock(CacheMutex);

		if (Cache.ModuleBase == Mem.ModBase &&
			Cache.Build == g_Offsets.CurrentBuild)
		{
			return Cache;
		}

		RocketBoostNatives Natives;
		Natives.ModuleBase = Mem.ModBase;
		Natives.Build = g_Offsets.CurrentBuild;

		// IDA: vehicle_commands::CommandSetRocketBoostFill(int, float).
		// ECX receives the script GUID and XMM1 receives the percentage.
		Natives.SetPercentage = Mem.FindSignatureStr(xorstr(
			"40 53 48 83 EC 40 0F 29 74 24 ? 0F 29 7C 24 ? 0F 28 F9"
		));

		// IDA: vehicle_commands::CommandSetScriptRocketBoostRechargeTime(int, float).
		// This is the native implementation, not a CVehicle field write.
		Natives.SetRefillTime = Mem.FindSignatureStr(xorstr(
			"40 53 48 83 EC 30 0F 29 74 24 ? 0F 28 F1 E8 ? ? ? ? "
			"48 8B D8 48 85 C0 74 ? 48 8B C8 E8 ? ? ? ? F3 0F 5E C6"
		));

		// Body fallbacks keep the resolver independent from fixed RVAs on builds
		// whose compiler changed only the function prologue.
		if (!Natives.SetPercentage)
		{
			const uintptr_t Body = Mem.FindSignatureStr(xorstr(
				"0F 57 F6 0F 2F FE 72 ? F3 0F 10 35 ? ? ? ? 0F 2F FE 77 ? "
				"0F 28 F7 48 8B CB E8 ? ? ? ? F3 0F 59 C6 F3 0F 11 83"
			));
			if (Body > Mem.ModBase + 0x2C)
				Natives.SetPercentage = Body - 0x2C;
		}

		if (!Natives.SetRefillTime)
		{
			const uintptr_t Body = Mem.FindSignatureStr(xorstr(
				"48 8B C8 E8 ? ? ? ? F3 0F 5E C6 F3 0F 11 83"
			));
			if (Body > Mem.ModBase + 0x1B)
				Natives.SetRefillTime = Body - 0x1B;
		}

		Cache = Natives;
		return Cache;
	}

	static bool ExecuteRocketBoostNatives(
		uintptr_t Vehicle,
		bool ApplyRefillTime,
		float RefillTime,
		bool ApplyPercentage,
		float Percentage)
	{
		if (!Vehicle || !g_Offsets.m_GetScriptGuidFromBase)
			return false;

		NetworkControl::Operation ControlOperation;
		if (!NetworkControl::HasControl(Vehicle))
			NetworkControl::RequestControl(Vehicle);

		const RocketBoostNatives Natives = GetRocketBoostNatives();
		if ((ApplyRefillTime && !Natives.SetRefillTime) ||
			(ApplyPercentage && !Natives.SetPercentage))
			return false;

		auto Append32 = [](std::vector<uint8_t>& Code, uint32_t Value)
		{
			for (int Index = 0; Index < 4; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};
		auto Append64 = [](std::vector<uint8_t>& Code, uint64_t Value)
		{
			for (int Index = 0; Index < 8; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};
		auto FloatBits = [](float Value)
		{
			uint32_t Bits = 0;
			std::memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		};

		std::vector<uint8_t> Code;
		Code.reserve(0x100);
		Code.insert(Code.end(), {0x48, 0x83, 0xEC, 0x28}); // sub rsp, 28h

		auto AppendNativeCall = [&](uintptr_t Native, float Value)
		{
			Code.insert(Code.end(), {0x48, 0xB9}); // mov rcx, vehicle
			Append64(Code, Vehicle);
			Code.insert(Code.end(), {0x48, 0xB8}); // mov rax, GetScriptGuidFromBase
			Append64(Code, g_Offsets.m_GetScriptGuidFromBase);
			Code.insert(Code.end(), {
				0xFF, 0xD0,                         // call rax
				0x8B, 0xC8,                         // mov ecx, eax
				0xB8                                // mov eax, float bits
			});
			Append32(Code, FloatBits(Value));
			Code.insert(Code.end(), {
				0x66, 0x0F, 0x6E, 0xC8,             // movd xmm1, eax
				0x48, 0xB8                          // mov rax, native
			});
			Append64(Code, Native);
			Code.insert(Code.end(), {0xFF, 0xD0}); // call rax
		};

		if (ApplyRefillTime)
			AppendNativeCall(Natives.SetRefillTime, RefillTime);
		if (ApplyPercentage)
			AppendNativeCall(Natives.SetPercentage, Percentage);

		Code.insert(Code.end(), {
			0x48, 0x83, 0xC4, 0x28,             // add rsp, 28h
			0xB8, 0x01, 0x00, 0x00, 0x00,       // mov eax, 1
			0xC3                                // ret
		});

		const uintptr_t Cave = Mem.CreateDataCave(0x200);
		if (!Cave)
			return false;
		const DWORD Result = Mem.execShellcode(Cave, Code.data(), Code.size());
		Mem.FreeCave(Cave, 0x200);
		return Result == 1;
	}

public:

	CVehicle* pNetObject() {
		if (!this || !g_Offsets.m_LastVehicle) return nullptr;
		return (CVehicle*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0xD0);
	}

	D3DXVECTOR3 GetPos( ) {
		if ( !this ) { return D3DXVECTOR3( 0, 0, 0 ); }
		return Mem.Read<D3DXVECTOR3>( reinterpret_cast< uintptr_t >( this ) + 0x90 );
	}

	void SetPos( D3DXVECTOR3 Pos ) {
		if ( !this ) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		Mem.Write<D3DXVECTOR3>( reinterpret_cast< uintptr_t >( this ) + 0x90, Pos );
	}

	uintptr_t GetHandling( ) {
		if ( !this ) { return 0; }
		return Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_Handling );
	}

	bool SetVehicleRocketBoostPercentage(float Percentage)
	{
		return ExecuteRocketBoostNatives(
			reinterpret_cast<uintptr_t>(this), false, 0.0f, true, Percentage);
	}

	bool SetVehicleRocketBoostRefillTime(float RefillTime)
	{
		return ExecuteRocketBoostNatives(
			reinterpret_cast<uintptr_t>(this), true, RefillTime, false, 0.0f);
	}

	bool RefillVehicleRocketBoost()
	{
		return ExecuteRocketBoostNatives(
			reinterpret_cast<uintptr_t>(this), true, 0.0f, true, 1.0f);
	}

private:
	struct VehicleModNatives {
		uintptr_t ModuleBase = 0;
		uint32_t Build = 0;
		uintptr_t SetVehicleModKit = 0;
		uintptr_t SetVehicleMod = 0;
	};

	static VehicleModNatives GetVehicleModNatives()
	{
		static std::mutex CacheMutex;
		static VehicleModNatives Cache;
		std::lock_guard<std::mutex> Lock(CacheMutex);

		if (Cache.ModuleBase == Mem.ModBase &&
			Cache.Build == g_Offsets.CurrentBuild)
			return Cache;

		VehicleModNatives Natives;
		Natives.ModuleBase = Mem.ModBase;
		Natives.Build = g_Offsets.CurrentBuild;

		// IDA b3258: vehicle_commands::CommandSetVehicleModKit em
		// 0x7FF76BA9DA4C. Signature cobre o prólogo + o acesso em
		// [rdx+2B8h] (que é word, campo modKitCount em CVehicleModelInfo).
		Natives.SetVehicleModKit = Mem.FindSignatureStr(xorstr(
			"53 48 83 EC 20 8B DA E8 ? ? ? ? 48 85 C0 74 ? "
			"48 8B 50 20 85 DB 78 ? 0F B7 8A"
		));

		// IDA b3258: vehicle_commands::CommandSetVehicleMod em
		// 0x7FF76BA9D8EC. O prólogo é único (3 pushes + sub rsp,30h
		// seguido do remap de modSlot via cmp edx,0Ah). O deslocamento
		// do jle (último byte) varia entre builds, então vai wildcard.
		Natives.SetVehicleMod = Mem.FindSignatureStr(xorstr(
			"48 89 5C 24 08 48 89 6C 24 18 56 57 41 56 48 83 EC 30 "
			"41 8A E9 45 8B F0 8B FA 83 FA 0A 7E ?"
		));

		Cache = Natives;
		return Cache;
	}

	// Shellcode:
	//   sub rsp, 28h
	//   ; call 1: SetVehicleModKit(guid, 0)
	//   mov rcx, vehicle
	//   mov rax, GetScriptGuidFromBase
	//   call rax
	//   mov ecx, eax
	//   xor edx, edx
	//   mov rax, SetVehicleModKit
	//   call rax
	//   ; call 2: SetVehicleMod(guid, modSlot, modIndex, 0)
	//   mov rcx, vehicle
	//   mov rax, GetScriptGuidFromBase
	//   call rax
	//   mov ecx, eax
	//   mov edx, modSlot
	//   mov r8d, modIndex
	//   xor r9d, r9d
	//   mov rax, SetVehicleMod
	//   call rax
	//   add rsp, 28h
	//   mov eax, 1
	//   ret
	static bool ExecuteSetVehicleMod(uintptr_t Vehicle, uint32_t ModSlot, uint8_t ModIndex)
	{
		if (!Vehicle || !g_Offsets.m_GetScriptGuidFromBase)
			return false;

		NetworkControl::Operation ControlOperation;
		if (!NetworkControl::HasControl(Vehicle))
			NetworkControl::RequestControl(Vehicle);

		const VehicleModNatives Natives = GetVehicleModNatives();
		if (!Natives.SetVehicleModKit || !Natives.SetVehicleMod)
			return false;

		auto Append32 = [](std::vector<uint8_t>& Code, uint32_t Value)
		{
			for (int Index = 0; Index < 4; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};
		auto Append64 = [](std::vector<uint8_t>& Code, uint64_t Value)
		{
			for (int Index = 0; Index < 8; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};

		std::vector<uint8_t> Code;
		Code.reserve(0x120);
		Code.insert(Code.end(), {0x48, 0x83, 0xEC, 0x28}); // sub rsp, 28h

		// --- SetVehicleModKit(guid, 0) ---
		Code.insert(Code.end(), {0x48, 0xB9}); // mov rcx, vehicle
		Append64(Code, Vehicle);
		Code.insert(Code.end(), {0x48, 0xB8}); // mov rax, GetScriptGuidFromBase
		Append64(Code, g_Offsets.m_GetScriptGuidFromBase);
		Code.insert(Code.end(), {
			0xFF, 0xD0,                   // call rax
			0x8B, 0xC8,                   // mov ecx, eax
			0x33, 0xD2                    // xor edx, edx (kitIndex=0)
		});
		Code.insert(Code.end(), {0x48, 0xB8}); // mov rax, SetVehicleModKit
		Append64(Code, Natives.SetVehicleModKit);
		Code.insert(Code.end(), {0xFF, 0xD0}); // call rax

		// --- SetVehicleMod(guid, modSlot, modIndex, false) ---
		Code.insert(Code.end(), {0x48, 0xB9}); // mov rcx, vehicle
		Append64(Code, Vehicle);
		Code.insert(Code.end(), {0x48, 0xB8}); // mov rax, GetScriptGuidFromBase
		Append64(Code, g_Offsets.m_GetScriptGuidFromBase);
		Code.insert(Code.end(), {
			0xFF, 0xD0,                   // call rax
			0x8B, 0xC8,                   // mov ecx, eax
			0xBA                          // mov edx, modSlot
		});
		Append32(Code, ModSlot);
		Code.insert(Code.end(), {0x41, 0xB8}); // mov r8d, modIndex
		Append32(Code, static_cast<uint32_t>(ModIndex));
		Code.insert(Code.end(), {0x45, 0x33, 0xC9}); // xor r9d, r9d
		Code.insert(Code.end(), {0x48, 0xB8}); // mov rax, SetVehicleMod
		Append64(Code, Natives.SetVehicleMod);
		Code.insert(Code.end(), {0xFF, 0xD0}); // call rax

		Code.insert(Code.end(), {
			0x48, 0x83, 0xC4, 0x28,       // add rsp, 28h
			0xB8, 0x01, 0x00, 0x00, 0x00, // mov eax, 1
			0xC3                          // ret
		});

		const uintptr_t Cave = Mem.CreateDataCave(0x200);
		if (!Cave) return false;
		const DWORD Result = Mem.execShellcode(Cave, Code.data(), Code.size());
		Mem.FreeCave(Cave, 0x200);
		return Result == 1;
	}

public:
	// Instala um upgrade de mod no veículo atual via a native
	// SET_VEHICLE_MOD do GTA. Antes chama SET_VEHICLE_MOD_KIT(0) para que
	// o veículo passe a aceitar peças de upgrade. ModSlot segue o enum
	// eVehicleModType; alguns úteis: 11=motor, 12=freios, 13=transmissão,
	// 15=suspensão, 16=armadura, 18=turbo.
	bool SetVehicleMod(uint32_t ModSlot, uint8_t ModIndex)
	{
		if (!this) return false;
		return ExecuteSetVehicleMod(reinterpret_cast<uintptr_t>(this), ModSlot, ModIndex);
	}

	// Equivalente a SET_VEHICLE_MOD(veh, 16, 4): armadura máxima oficial.
	// Só funciona em veículos com kit de modificação (praticamente todos
	// os civis/esportivos; alguns únicos/missão não aceitam e a chamada
	// vira no-op dentro do próprio jogo — não trava nada).
	bool InstallMaxArmorMod()
	{
		return SetVehicleMod(16u, 4u);
	}

	uint64_t GetModelInfo()
	{
		if (!this)	return 0;
		return Mem.Read<uint64_t>(reinterpret_cast<uint64_t>(this) + 0x20);
	}

	private:
	struct VehiclePlateNatives
	{
		uintptr_t ModuleBase = 0;
		uint32_t Build = 0;
		uintptr_t GetText = 0;
		uintptr_t SetText = 0;
	};

	static VehiclePlateNatives GetVehiclePlateNatives()
	{
		static std::mutex CacheMutex;
		static VehiclePlateNatives Cache;
		std::lock_guard<std::mutex> Lock(CacheMutex);

		if (Cache.ModuleBase == Mem.ModBase && Cache.Build == g_Offsets.CurrentBuild)
			return Cache;

		VehiclePlateNatives Natives;
		Natives.ModuleBase = Mem.ModBase;
		Natives.Build = g_Offsets.CurrentBuild;

		// Wrappers GET/SET_VEHICLE_NUMBER_PLATE_TEXT. As assinaturas usam a
		// cadeia CVehicle+48h -> DrawHandler+20h e deixam a codificacao interna
		// da placa a cargo de CCustomShaderEffectVehicle.
		Natives.GetText = Mem.FindSignatureStr(xorstr(
			"48 83 EC 28 E8 ? ? ? ? 48 85 C0 74 ? 48 8B 40 48 "
			"48 8B 48 20 48 83 C4 28 E9 ? ? ? ? 48 83 C4 28 C3"
		));
		Natives.SetText = Mem.FindSignatureStr(xorstr(
			"40 53 48 83 EC 20 48 8B DA E8 ? ? ? ? 48 85 C0 74 ? "
			"48 8B 48 48 48 8B D3 48 8B 49 20 E8 ? ? ? ? 48 83 C4 20 5B C3"
		));

		Cache = Natives;
		return Cache;
	}

	static std::mutex& VehiclePlateOperationMutex()
	{
		static std::mutex Mutex;
		return Mutex;
	}

	public:
	std::string GetPlateText()
	{
		if (!this || !g_Offsets.m_GetScriptGuidFromBase)
			return {};

		std::lock_guard<std::mutex> Lock(VehiclePlateOperationMutex());
		const VehiclePlateNatives Natives = GetVehiclePlateNatives();
		if (!Natives.GetText)
			return {};

		const uintptr_t CodeCave = Mem.CreateDataCave(0x100);
		const uintptr_t ResultCave = Mem.CreateDataCave(16);
		if (!CodeCave || !ResultCave)
		{
			if (CodeCave) Mem.FreeCave(CodeCave, 0x100);
			if (ResultCave) Mem.FreeCave(ResultCave, 16);
			return {};
		}

		auto Append64 = [](std::vector<uint8_t>& Code, uint64_t Value)
		{
			for (int Index = 0; Index < 8; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};

		std::vector<uint8_t> Code;
		Code.reserve(0x80);
		Code.insert(Code.end(), {0x48, 0x83, 0xEC, 0x28}); // sub rsp,28h
		Code.insert(Code.end(), {0x48, 0xB9});              // mov rcx,vehicle
		Append64(Code, reinterpret_cast<uintptr_t>(this));
		Code.insert(Code.end(), {0x48, 0xB8});              // mov rax,GetScriptGuidFromBase
		Append64(Code, g_Offsets.m_GetScriptGuidFromBase);
		Code.insert(Code.end(), {0xFF, 0xD0, 0x8B, 0xC8});  // call rax; mov ecx,eax
		Code.insert(Code.end(), {0x48, 0xB8});              // mov rax,GET_VEHICLE_NUMBER_PLATE_TEXT
		Append64(Code, Natives.GetText);
		Code.insert(Code.end(), {
			0xFF, 0xD0,                         // call rax
			0x48, 0x85, 0xC0,                   // test rax,rax
			0x75, 0x07,                         // jnz copy_result
			0x33, 0xC0,                         // xor eax,eax
			0x48, 0x83, 0xC4, 0x28,             // add rsp,28h
			0xC3                                // ret
		});
		Code.insert(Code.end(), {0x48, 0xBA});              // mov rdx,result
		Append64(Code, ResultCave);
		Code.insert(Code.end(), {
			0x48, 0x8B, 0x08,                   // mov rcx,[rax]
			0x48, 0x89, 0x0A,                   // mov [rdx],rcx
			0x8A, 0x48, 0x08,                   // mov cl,[rax+8]
			0x88, 0x4A, 0x08,                   // mov [rdx+8],cl
			0x48, 0x83, 0xC4, 0x28,             // add rsp,28h
			0xB8, 0x01, 0x00, 0x00, 0x00,       // mov eax,1
			0xC3                                // ret
		});

		char Text[9]{};
		const DWORD Executed = Mem.execShellcode(CodeCave, Code.data(), Code.size());
		const bool Read = Executed == 1 && Mem.ReadBuffer(ResultCave, Text, sizeof(Text));
		Mem.FreeCave(CodeCave, 0x100);
		Mem.FreeCave(ResultCave, 16);
		if (!Read)
			return {};

		Text[8] = '\0';
		std::string Result(Text);
		while (!Result.empty() && Result.back() == ' ')
			Result.pop_back();
		while (!Result.empty() && Result.front() == ' ')
			Result.erase(Result.begin());
		return Result;
	}

	bool SetPlateText(const char* Text)
	{
		if (!this || !Text || !g_Offsets.m_GetScriptGuidFromBase)
			return false;

		std::lock_guard<std::mutex> Lock(VehiclePlateOperationMutex());
		const VehiclePlateNatives Natives = GetVehiclePlateNatives();
		if (!Natives.SetText)
			return false;

		char Normalized[9]{};
		for (size_t Index = 0; Index < 8 && Text[Index]; ++Index)
		{
			const char Character = Text[Index];
			Normalized[Index] = Character >= 'a' && Character <= 'z'
				? static_cast<char>(Character - 'a' + 'A')
				: Character;
		}

		NetworkControl::Operation ControlOperation;
		const uintptr_t Vehicle = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(Vehicle))
			NetworkControl::RequestControl(Vehicle);

		const uintptr_t CodeCave = Mem.CreateDataCave(0x100);
		const uintptr_t TextCave = Mem.CreateDataCave(sizeof(Normalized));
		if (!CodeCave || !TextCave ||
			!Mem.WriteBytesImpl(TextCave, Normalized, sizeof(Normalized)))
		{
			if (CodeCave) Mem.FreeCave(CodeCave, 0x100);
			if (TextCave) Mem.FreeCave(TextCave, sizeof(Normalized));
			return false;
		}

		auto Append64 = [](std::vector<uint8_t>& Code, uint64_t Value)
		{
			for (int Index = 0; Index < 8; ++Index)
				Code.push_back(static_cast<uint8_t>(Value >> (Index * 8)));
		};

		std::vector<uint8_t> Code;
		Code.reserve(0x70);
		Code.insert(Code.end(), {0x48, 0x83, 0xEC, 0x28}); // sub rsp,28h
		Code.insert(Code.end(), {0x48, 0xB9});              // mov rcx,vehicle
		Append64(Code, Vehicle);
		Code.insert(Code.end(), {0x48, 0xB8});              // mov rax,GetScriptGuidFromBase
		Append64(Code, g_Offsets.m_GetScriptGuidFromBase);
		Code.insert(Code.end(), {0xFF, 0xD0, 0x8B, 0xC8});  // call rax; mov ecx,eax
		Code.insert(Code.end(), {0x48, 0xBA});              // mov rdx,text
		Append64(Code, TextCave);
		Code.insert(Code.end(), {0x48, 0xB8});              // mov rax,SET_VEHICLE_NUMBER_PLATE_TEXT
		Append64(Code, Natives.SetText);
		Code.insert(Code.end(), {
			0xFF, 0xD0,                         // call rax
			0x48, 0x83, 0xC4, 0x28,             // add rsp,28h
			0xB8, 0x01, 0x00, 0x00, 0x00,       // mov eax,1
			0xC3                                // ret
		});

		const DWORD Result = Mem.execShellcode(CodeCave, Code.data(), Code.size());
		Mem.FreeCave(CodeCave, 0x100);
		Mem.FreeCave(TextCave, sizeof(Normalized));
		return Result == 1;
	}

	void SetExtras(uint32_t value) {
		if (!this || !g_Offsets.m_VehicleExtras) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);

		Mem.Write<uint32_t>((uintptr_t)this + g_Offsets.m_VehicleExtras, value);
	}

	bool GetGodMode( ) {
		if ( !this ) { return false; }
		return Mem.Read<BYTE>( reinterpret_cast< uintptr_t >( this ) + 0x189 );
	}

	void SetGodMode( bool Toggle ) {
		if ( !this ) { return; }
		Mem.Write<BYTE>( reinterpret_cast< uintptr_t >( this ) + 0x189, Toggle );
	}

	float GetGravity( ) {
		if ( !this ) { return 0; }
		return Mem.Read<float>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_VehicleGravity );
	}

	void SetGravity( float Value ) {
		if ( !this ) { return; }
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		Mem.Write<float>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_VehicleGravity, Value );
	}

	uintptr_t GetNavigation() {
		if (!this) { return 0; }
		return Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x30);
	}

	void Fix(CPed* localPlayer) {
		if (!this || !localPlayer) return;

		uintptr_t vehiclePtr = reinterpret_cast<uintptr_t>(this);
		uintptr_t localPedPtr = reinterpret_cast<uintptr_t>(localPlayer);
		uintptr_t engineHealthOff = g_Offsets.m_VehicleEngineHealth;
		uintptr_t stateOff = g_Offsets.m_VehicleState;

		std::thread([vehiclePtr, localPedPtr, engineHealthOff, stateOff]()
		{
			NetworkControl::Operation ControlOperation;
			if (!NetworkControl::HasControl(vehiclePtr))
				NetworkControl::RequestControl(vehiclePtr);

			auto push8 = [](std::vector<uint8_t>& v, uint8_t b) { v.push_back(b); };
			auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
				for (int i = 0; i < 4; i++) v.push_back((val >> (i * 8)) & 0xFF);
			};
			auto push64 = [](std::vector<uint8_t>& v, uint64_t val) {
				for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
			};
			auto pushJunk = [&](std::vector<uint8_t>& v) {
				int n = 1 + (std::rand() % 2);
				for (int j = 0; j < n; j++) {
					switch (std::rand() % 6) {
					case 0: push8(v, 0x90); break;
					case 1: push8(v, 0x66); push8(v, 0x90); break;
					case 2: push8(v, 0x0F); push8(v, 0x1F); push8(v, 0x00); break;
					case 3: push8(v, 0x48); push8(v, 0x87); push8(v, 0xC0); break;
					case 4: push8(v, 0x49); push8(v, 0xBA); push64(v, (uint64_t)std::rand()); break;
					case 5: push8(v, 0x49); push8(v, 0xBB); push64(v, (uint64_t)std::rand()); break;
					}
				}
			};

			std::vector<uint8_t> sc;
			sc.reserve(0x100);

			pushJunk(sc);
			push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xEC); push8(sc, 0x28);
			pushJunk(sc);
			push8(sc, 0x48); push8(sc, 0xB8); push64(sc, vehiclePtr + engineHealthOff);
			push8(sc, 0xC7); push8(sc, 0x00); push32(sc, 0x447A0000);
			pushJunk(sc);
			push8(sc, 0x48); push8(sc, 0xB8); push64(sc, vehiclePtr + 0x280);
			push8(sc, 0xC7); push8(sc, 0x00); push32(sc, 0x447A0000);
			pushJunk(sc);
			push8(sc, 0x48); push8(sc, 0xB8); push64(sc, vehiclePtr + stateOff);
			push8(sc, 0xC7); push8(sc, 0x00); push32(sc, 0x17);
			pushJunk(sc);
			push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xC4); push8(sc, 0x28);
			push8(sc, 0x31); push8(sc, 0xC0);
			push8(sc, 0xC3);

			uintptr_t cave = Core::Mem.CreateDataCave(0x200);
			if (cave)
			{
				Mem.execShellcode(cave, sc.data(), sc.size());
				Core::Mem.FreeCave(cave, sc.size());
			}
		}).detach();
	}

	float GetHealth()
	{
		if (!this)
			return 0.f;

		return Mem.Read<float>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_VehicleEngineHealth);
	}

	float SetHealth(float value)
	{
		if (!this)
			return 0;

		float ZeroValue = -4000.0f;

		static const int Fix2Addr = Mem.Read<int>(Mem.FindSignature(
			{ 0xf3, 0x0f, 0x10, 0x80, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x89, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x28, 0xda }
		) + 4);

		static const int Fix3Addr = Mem.Read<int>(Mem.FindSignature(
			{ 0xf3, 0x0f, 0x10, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x81 }
		) + 4);

		static const int Fix4Addr = Mem.Read<int>(Mem.FindSignature(
			{ 0x8a, 0x87, 0x00 , 0x00 , 0x00 , 0x00 , 0xc0, 0xe8, 0x00 , 0x88, 0x82, 0x00 , 0x00 , 0x00 , 0x00 , 0x8a, 0x87, 0x00 , 0x00 , 0x00 , 0x00 , 0xc0, 0xe8, 0x00 , 0x41, 0x22, 0xc6, 0x88, 0x82, 0x00 , 0x00 , 0x00 , 0x00 , 0x48, 0x8b, 0x87 }
		) + 2);

		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + 0x280, ZeroValue);
		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + Fix2Addr, ZeroValue);
		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_VehicleEngineHealth, ZeroValue);
		Mem.Write<float>(reinterpret_cast<uintptr_t>(this) + Fix3Addr, ZeroValue);

		Mem.Write<uint8_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_VehicleState, static_cast<uint8_t>(0x00));
	}

	bool IsLocked( ) {
		if ( !this ) { return false; }
		return Mem.Read<uint32_t>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_VehicleDoorsLockState ) == 2;
	}

	void DoorState(eVehicleLockState state)
	{
		if (!this) { return; }
		Mem.Write<uint32_t>(reinterpret_cast<uintptr_t>(this) + g_Offsets.m_VehicleDoorsLockState,static_cast<uint32_t>(state) );
	}

	D3DXVECTOR3 GetVelocity( ) {
		if ( !this ) { return D3DXVECTOR3( 0, 0, 0 ); }
		return Mem.Read<D3DXVECTOR3>( reinterpret_cast< uintptr_t >( this ) + 0x320 );
	}

	void SetVelocity( D3DXVECTOR3 Vel ) {
		if ( !this ) return;
		uintptr_t vehAddr = reinterpret_cast< uintptr_t >( this );
		uintptr_t nav = Mem.Read<uintptr_t>( vehAddr + 0x30 );
		Mem.Write<D3DXVECTOR3>( vehAddr + 0x320, Vel );
		if ( nav )
			Mem.Write<D3DXVECTOR3>( nav + 0x30, Vel );
	}

	void SetPosFull( D3DXVECTOR3 Pos ) {
		if ( !this ) return;
		uintptr_t vehAddr = reinterpret_cast< uintptr_t >( this );
		uintptr_t nav = Mem.Read<uintptr_t>( vehAddr + 0x30 );
		Mem.Write<D3DXVECTOR3>( vehAddr + 0x90, Pos );
		if ( nav )
			Mem.Write<D3DXVECTOR3>( nav + 0x50, Pos );
	}

	void Freeze(bool state)
	{
		if (!this) return;
		uintptr_t vehPtr = reinterpret_cast<uintptr_t>(this);
		
		NetworkControl::Operation ControlOperation;
		if (!NetworkControl::HasControl(vehPtr))
		{
			NetworkControl::RequestControl(vehPtr);
			// Wait for control confirmation
			auto startTime = std::chrono::steady_clock::now();
			while (!NetworkControl::HasControl(vehPtr))
			{
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - startTime).count();
				if (elapsed >= 2000) return; // Timeout after 2s
				Sleep(50);
			}
		}
		
		// m_baseFlags at +0x2E: 1=freeze, 2=unfreeze (confirmed in FreezePed)
		Mem.Write<uint8_t>(vehPtr + 0x2E, state ? 1 : 2);
	}

	void SetVelocityTowards(const D3DXVECTOR3& direction, float force)
	{
		if (!this || !g_Offsets.m_SetPhysicalVelocity) return;
		
		uintptr_t vehPtr = reinterpret_cast<uintptr_t>(this);
		
		// Network control check
		if (!NetworkControl::HasControl(vehPtr))
			return; // Do NOT write without control
		
		// Normalize direction
		D3DXVECTOR3 dir = direction;
		float len = D3DXVec3Length(&dir);
		if (len < 0.001f) return;
		D3DXVec3Normalize(&dir, &dir);
		
		// Apply force
		D3DXVECTOR3 velocity = dir * force;
		
		// Call CPhysical::SetVelocity via m_SetPhysicalVelocity
		const uintptr_t setVel = g_Offsets.m_SetPhysicalVelocity;
		std::thread([vehPtr, velocity, setVel]()
		{
			const uintptr_t cave = Mem.CreateDataCave(0x100);
			if (!cave) return;
			
			Mem.Write<D3DXVECTOR3>(cave + 0x80, velocity);
			
			std::vector<uint8_t> code = {0x48, 0x83, 0xEC, 0x28, 0x48, 0xB9};
			for (int i = 0; i < 8; ++i)
				code.push_back((uint8_t)(vehPtr >> (i * 8)));
			code.insert(code.end(), {0x48, 0xBA});
			uintptr_t vecAddr = cave + 0x80;
			for (int i = 0; i < 8; ++i)
				code.push_back((uint8_t)(vecAddr >> (i * 8)));
			code.insert(code.end(), {0x48, 0xB8});
			for (int i = 0; i < 8; ++i)
				code.push_back((uint8_t)(setVel >> (i * 8)));
			code.insert(code.end(), {0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28, 0x33, 0xC0, 0xC3});
			
			Mem.execShellcode(cave, code.data(), code.size());
			Mem.FreeCave(cave, 0x100);
		}).detach();
	}

	CPed * GetDriver( )
	{
		if ( !this ) return 0;
		return ( CPed * )Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + g_Offsets.m_VehicleDriver );
	}

	uint8_t GetOwnerId() const {
		return Core::Mem.Read<uint8_t>((uintptr_t)this + 0x49);
	}

	void SetOwnerId(uint8_t ownerId) {
		Core::Mem.Write<uint8_t>((uintptr_t)this + 0x49, ownerId);
	}
	void NetworkRequestControl(CPed* localPlayer)
	{
		if (!this || !localPlayer) return;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
	}

	bool HasDriver()
	{
		if (!this) return false;
		return GetDriver() != nullptr;
	}

	void PutPedInSeatUD(CPed* ped, int seat)
	{
		if (!this || !ped) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		const Entity pedEntity = reinterpret_cast<uintptr_t>(ped);
		if (ped != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(pedEntity))
			NetworkControl::RequestControl(pedEntity);
		uintptr_t vehiclePtr = reinterpret_cast<uintptr_t>(this);
		uintptr_t pedAddr = reinterpret_cast<uintptr_t>(ped);

		if (seat >= 0 && seat < 16) {
			Mem.Write<uintptr_t>(vehiclePtr + g_Offsets.m_VehicleDriver + 8 * seat, pedAddr);
			Mem.Write<uintptr_t>(pedAddr + g_Offsets.m_LastVehicle, vehiclePtr);
			ped->SetConfigFlag(ePedConfigFlag::InVehicle, true);
		}
	}

	void RemovePedFromSeatUD(CPed* ped)
	{
		if (!this || !ped) return;
		NetworkControl::Operation ControlOperation;
		const Entity entity = reinterpret_cast<uintptr_t>(this);
		if (!NetworkControl::HasControl(entity))
			NetworkControl::RequestControl(entity);
		const Entity pedEntity = reinterpret_cast<uintptr_t>(ped);
		if (ped != Core::SDK::Pointers::pLocalPlayer && !NetworkControl::HasControl(pedEntity))
			NetworkControl::RequestControl(pedEntity);
		uintptr_t vehiclePtr = reinterpret_cast<uintptr_t>(this);
		uintptr_t pedAddr = reinterpret_cast<uintptr_t>(ped);

		for (int i = 0; i < 16; i++) {
			if (Mem.Read<uintptr_t>(vehiclePtr + g_Offsets.m_VehicleDriver + 8 * i) == pedAddr) {
				Mem.Write<uintptr_t>(vehiclePtr + g_Offsets.m_VehicleDriver + 8 * i, 0);
				break;
			}
		}
		Mem.Write<uintptr_t>(pedAddr + g_Offsets.m_LastVehicle, 0);
		ped->SetConfigFlag(ePedConfigFlag::InVehicle, false);
	}

	void StealDriverSeat(CPed* localPlayer)
	{
		if (!this || !localPlayer) return;

		static std::atomic<bool> g_StealInProgress{false};
		bool expected = false;
		if (!g_StealInProgress.compare_exchange_strong(expected, true))
			return;

		uintptr_t vehiclePtr = reinterpret_cast<uintptr_t>(this);
		uintptr_t localPedPtr = reinterpret_cast<uintptr_t>(localPlayer);
		const auto ControlRequests = NetworkControl::RequestedEntities;

		std::thread([vehiclePtr, localPedPtr, ControlRequests]()
		{
			CVehicle* veh = reinterpret_cast<CVehicle*>(vehiclePtr);
			CPed* localPed = reinterpret_cast<CPed*>(localPedPtr);

			NetworkControl::Operation ControlOperation(ControlRequests);
			if (!NetworkControl::HasControl(vehiclePtr))
				NetworkControl::RequestControl(vehiclePtr);

			constexpr int passengerSeat = 1;
			const uintptr_t passengerSlot = vehiclePtr + g_Offsets.m_VehicleDriver + 8 * passengerSeat;
			const uintptr_t passenger = Mem.Read<uintptr_t>(passengerSlot);
			if (veh->GetDriver() == localPed || (passenger && passenger != localPedPtr))
			{
				g_StealInProgress = false;
				return;
			}

			if (g_Offsets.m_SetPedInVehicleFunc)
			{
				if (!localPed->SetInVehicle(vehiclePtr, passengerSeat, 0x20))
				{
					g_StealInProgress = false;
					return;
				}
			}
			else
			{
				veh->PutPedInSeatUD(localPed, passengerSeat);
			}

			const auto passengerEntryStart = std::chrono::steady_clock::now();
			while (Mem.Read<uintptr_t>(passengerSlot) != localPedPtr)
			{
				if (std::chrono::steady_clock::now() - passengerEntryStart >= std::chrono::seconds(2))
				{
					g_StealInProgress = false;
					return;
				}
				Sleep(50);
			}
			Sleep(250);

			CPed* driver = veh->GetDriver();
			uintptr_t driverPtr = reinterpret_cast<uintptr_t>(driver);
			if (driverPtr && driverPtr != localPedPtr)
			{
				veh->RemovePedFromSeatUD(driver);
				Sleep(100);
			}

			if (g_Offsets.m_SetPedInVehicleFunc)
			{
				localPed->SetInVehicle(vehiclePtr, 0, 0x20);
			}
			else
			{
				veh->PutPedInSeatUD(localPed, 0);
				if (Mem.Read<uintptr_t>(passengerSlot) == localPedPtr)
					Mem.Write<uintptr_t>(passengerSlot, 0);
			}

			g_StealInProgress = false;
		}).detach();
	}

	void BreakWheels(CPed* localPlayer)
	{
		if (!this || !localPlayer) return;
		if (!g_Offsets.m_BreakOffWheel) return;

		uintptr_t vehiclePtr = reinterpret_cast<uintptr_t>(this);
		uintptr_t localPedPtr = reinterpret_cast<uintptr_t>(localPlayer);
		uintptr_t breakOffFunc = g_Offsets.m_BreakOffWheel;

		NetworkControl::Operation ControlOperation;
		if (!NetworkControl::HasControl(vehiclePtr))
			NetworkControl::RequestControl(vehiclePtr);

		StealDriverSeat(localPlayer);

		std::thread([vehiclePtr, localPedPtr, breakOffFunc]()
		{
			auto startTime = std::chrono::steady_clock::now();
			while (true)
			{
				uintptr_t currentDriver = Mem.Read<uintptr_t>(vehiclePtr + g_Offsets.m_VehicleDriver);
				if (currentDriver == localPedPtr) break;

				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::steady_clock::now() - startTime).count();
				if (elapsed >= 4000) break;

				Sleep(100);
			}

			uintptr_t damageStruct = vehiclePtr + 0x420;
			int numberOfWheels = Mem.Read<int>(vehiclePtr + 0xC28);
			if (numberOfWheels <= 0 || numberOfWheels > 12)
				numberOfWheels = 4;

			static std::mt19937 rng(std::random_device{}());

			auto push8 = [](std::vector<uint8_t>& v, uint8_t b) { v.push_back(b); };
			auto push32 = [](std::vector<uint8_t>& v, uint32_t val) {
				for (int i = 0; i < 4; i++) v.push_back((val >> (i * 8)) & 0xFF);
			};
			auto push64 = [](std::vector<uint8_t>& v, uint64_t val) {
				for (int i = 0; i < 8; i++) v.push_back((val >> (i * 8)) & 0xFF);
			};
			auto pushJunk = [&](std::vector<uint8_t>& v) {
				int n = 1 + (std::rand() % 2);
				for (int j = 0; j < n; j++) {
					switch (std::rand() % 6) {
					case 0: push8(v, 0x90); break;
					case 1: push8(v, 0x66); push8(v, 0x90); break;
					case 2: push8(v, 0x0F); push8(v, 0x1F); push8(v, 0x00); break;
					case 3: push8(v, 0x48); push8(v, 0x87); push8(v, 0xC0); break;
					case 4: push8(v, 0x49); push8(v, 0xBA); push64(v, (uint64_t)std::rand()); break;
					case 5: push8(v, 0x49); push8(v, 0xBB); push64(v, (uint64_t)std::rand()); break;
					}
				}
			};

			for (int i = 0; i < numberOfWheels; i++)
			{
				std::vector<uint8_t> sc;
				sc.reserve(0x100);

				pushJunk(sc);
				// sub rsp, 0x48
				push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xEC); push8(sc, 0x48);
				pushJunk(sc);
				// mov rcx, damageStruct
				push8(sc, 0x48); push8(sc, 0xB9); push64(sc, damageStruct);
				pushJunk(sc);
				// mov edx, wheelIndex
				push8(sc, 0xBA); push32(sc, (uint32_t)i);
				pushJunk(sc);
				// xorps xmm2, xmm2 (ptfxProbability = 0.0)
				push8(sc, 0x0F); push8(sc, 0x57); push8(sc, 0xD2);
				// xorps xmm3, xmm3 (deleteProbability = 0.0)
				push8(sc, 0x0F); push8(sc, 0x57); push8(sc, 0xDB);
				// [rsp+0x20] = 0.0f (burstTyreProbability)
				push8(sc, 0xC7); push8(sc, 0x44); push8(sc, 0x24); push8(sc, 0x20); push32(sc, 0);
				// [rsp+0x28] = 0 (dueToExplosion = false)
				push8(sc, 0xC7); push8(sc, 0x44); push8(sc, 0x24); push8(sc, 0x28); push32(sc, 0);
				// [rsp+0x30] = 1 (bNetworkCheck = true — triggers network sync)
				push8(sc, 0xC7); push8(sc, 0x44); push8(sc, 0x24); push8(sc, 0x30); push32(sc, 1);
				pushJunk(sc);
				// mov rax, breakOffFunc
				push8(sc, 0x48); push8(sc, 0xB8); push64(sc, breakOffFunc);
				// call rax
				push8(sc, 0xFF); push8(sc, 0xD0);
				// add rsp, 0x48
				push8(sc, 0x48); push8(sc, 0x83); push8(sc, 0xC4); push8(sc, 0x48);
				// ret
				push8(sc, 0xC3);

				uintptr_t cave = Core::Mem.CreateDataCave(0x200);
				if (cave)
				{
					Mem.execShellcode(cave, sc.data(), sc.size());
					Core::Mem.FreeCave(cave, sc.size());
				}
				std::uniform_int_distribution<int> dist(40, 100);
				Sleep(dist(rng));
			}
		}).detach();
	}
};

class CWeaponManager {
public:
	CWeaponInfo* GetWeaponInfo() {
		if (!this) { return 0; }
		return (CWeaponInfo*)Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x20);
	}

	uintptr_t GetCurrentWeapon() {
		if (!this) return 0;
		return Mem.Read<uintptr_t>(reinterpret_cast<uintptr_t>(this) + 0x20);
	}

	uint64_t GetWeaponHash() {
		uintptr_t weapon = GetCurrentWeapon();
		if (!weapon) return 0;
		return Mem.Read<uint64_t>(weapon + 0x10);
	}

	float GetRecoil() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + g_Offsets.m_Recoil);
	}

	float SetRecoil(float Recoil) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Write<float>((uintptr_t)WeaponInfo + g_Offsets.m_Recoil, Recoil);
	}

	float GetSpread() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + g_Offsets.m_Spread);
	}

	float SetSpread(float Spread) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Write<float>((uintptr_t)WeaponInfo + g_Offsets.m_Spread, Spread);
	}

	float GetReload() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + g_Offsets.m_Reload);
	}

	float SetReload(float Reload) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Write<float>((uintptr_t)WeaponInfo + g_Offsets.m_Reload, Reload);
	}

	float GetAmmoType() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + 0x20);
	}

	float SetAmmoType(float Type) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Write<float>((uintptr_t)WeaponInfo + 0x20, Type);
	}

	float GetDamage() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + 0xB0);
	}

	float SetDamage(float Damage) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Write<float>((uintptr_t)WeaponInfo + 0xB0, Damage);
	}

	std::pair<float, float> GetSizes() {
		if (!this) { return { 0.0f, 0.0f }; }

		auto CObject = Mem.Read<uintptr_t>((uintptr_t)this + g_Offsets.m_CObject);

		float size1 = Mem.Read<float>((uintptr_t)CObject + 0x7C);
		float size2 = Mem.Read<float>((uintptr_t)CObject + 0x8C);

		return { size1, size2 };
	}

	float SetSize(float size) {
		if (!this) { return 0.0f; }
		auto CObject = Mem.Read<uintptr_t>((uintptr_t)this + g_Offsets.m_CObject);
		if (!CObject) return 0.0f;
		const bool First = Mem.Write<float>((uintptr_t)CObject + 0x7C, size) != FALSE;
		const bool Second = Mem.Write<float>((uintptr_t)CObject + 0x8C, size) != FALSE;
		return First && Second ? size : 0.0f;
	}

	float GetBulletSpeed() {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		return Mem.Read<float>((uintptr_t)WeaponInfo + 0x11C);
	}

	float GetRange() {
		if (!this) return 0.0f;

		CWeaponInfo* WeaponInfo = reinterpret_cast<CWeaponInfo*>(GetWeaponInfo());
		if (!WeaponInfo) return 0.0f;

		uintptr_t offset = 0;

		if (g_Offsets.CurrentBuild == 1604)
			offset = 0x25C;
		else if (g_Offsets.CurrentBuild > 1604)
			offset = 0x28C;
		else
			return 0x0;

		return Mem.Read<float>(reinterpret_cast<uintptr_t>(WeaponInfo) + offset);
	}

	float SetRange(float Damage) {
		if (!this) { return 0.0f; }
		CWeaponInfo* WeaponInfo = (CWeaponInfo*)GetWeaponInfo();
		uintptr_t offset = 0;

		if (g_Offsets.CurrentBuild == 1604)
			offset = 0x25C;
		else if (g_Offsets.CurrentBuild > 1604)
			offset = 0x28C;
		else
			return 0x0;

		return Mem.Write<float>((uintptr_t)WeaponInfo + offset, Damage);
	}

};


class CPedFactory {
public:
	CPed * GetLocalPlayer( ) {
		if ( !this ) { return 0; }
		return ( CPed * ) Mem.Read<uintptr_t>( reinterpret_cast< uintptr_t >( this ) + 0x8 );
	}
};
