// RoomActorConsoleCommands.cpp
#include "RoomActors/RoomActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#if ! UE_BUILD_SHIPPING

/**
 * Console command:  Room. SetDebugMode [0-7]
 * Sets visualization mode for all RoomActors in level
 */
static FAutoConsoleCommand CCmdSetRoomDebugMode(
	TEXT("Room.SetDebugMode"),
	TEXT("Set room debug visualization mode (0=None, 1=Simple, 2=Detailed, 3=CellTypes, 4=Walls, 5=Topology, 6=Connections, 7=All)"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() == 0 || ! GEngine || !GEngine->GetWorld())
		{
			UE_LOG(LogTemp, Warning, TEXT("Usage: Room.SetDebugMode [0-7]"));
			return;
		}

		int32 ModeIndex = FCString::Atoi(*Args[0]);
		ModeIndex = FMath::Clamp(ModeIndex, 0, 7);
		EDebugVisualizationMode Mode = static_cast<EDebugVisualizationMode>(ModeIndex);

		int32 RoomsAffected = 0;
		for (TActorIterator<ARoomActor> It(GEngine->GetWorld()); It; ++It)
		{
			ARoomActor* RoomActor = *It;
			if (RoomActor && RoomActor->DebugHelpers)
			{
				RoomActor->DebugHelpers->SetVisualizationMode(Mode);
				RoomActor->RefreshVisualization();
				RoomsAffected++;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Set debug mode to %d for %d rooms"), ModeIndex, RoomsAffected);
	})
);

/**
 * Console command:  Room. ToggleDebug
 * Toggle debug visualization on/off for all RoomActors
 */
static FAutoConsoleCommand CCmdToggleRoomDebug(
	TEXT("Room. ToggleDebug"),
	TEXT("Toggle room debug visualization on/off"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (! GEngine || !GEngine->GetWorld())
		{
			return;
		}

		int32 RoomsAffected = 0;
		for (TActorIterator<ARoomActor> It(GEngine->GetWorld()); It; ++It)
		{
			ARoomActor* RoomActor = *It;
			if (RoomActor && RoomActor->DebugHelpers)
			{
				RoomActor->DebugHelpers->bEnableDebug = !RoomActor->DebugHelpers->bEnableDebug;
				RoomActor->RefreshVisualization();
				RoomsAffected++;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Toggled debug for %d rooms"), RoomsAffected);
	})
);

/**
 * Console command:  Room.RefreshDebug
 * Refresh debug visualization for all RoomActors
 */
static FAutoConsoleCommand CCmdRefreshRoomDebug(
	TEXT("Room.RefreshDebug"),
	TEXT("Refresh room debug visualization"),
	FConsoleCommandDelegate::CreateLambda([]()
	{
		if (!GEngine || !GEngine->GetWorld())
		{
			return;
		}

		int32 RoomsAffected = 0;
		for (TActorIterator<ARoomActor> It(GEngine->GetWorld()); It; ++It)
		{
			ARoomActor* RoomActor = *It;
			if (RoomActor && RoomActor->IsRoomGenerated())
			{
				RoomActor->RefreshVisualization();
				RoomsAffected++;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Refreshed debug for %d rooms"), RoomsAffected);
	})
);

#endif // !UE_BUILD_SHIPPING