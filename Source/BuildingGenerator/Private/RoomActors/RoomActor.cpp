// Fill out your copyright notice in the Description page of Project Settings.

#include "RoomActors/RoomActor.h"
#include "Generators/Rooms/RoomGenerator.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/Generation/RoomGenerationTypes.h"
#include "Data/Room/DoorData.h" 
#include "RoomActors/Doorway.h"
#include "Utilities/Generation/RoomGenerationHelpers.h"
#include "Utilities/Spawners/RoomSpawnerHelpers.h" 

// Sets default values
ARoomActor::ARoomActor()
{
 	// Set this actor to call Tick() every frame (can turn off for performance)
	PrimaryActorTick.bCanEverTick = false;
	
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// Create debug helpers component
	DebugHelpers = CreateDefaultSubobject<UDebugHelpers>(TEXT("DebugHelpers"));

	// Bind delegate so DebugHelpers can request text components
	DebugHelpers->OnCreateTextComponent.BindUObject(this, &ARoomActor::CreateTextRenderComponent);

	// Bind destruction delegate
	DebugHelpers->OnDestroyTextComponent. BindUObject(this, &ARoomActor::DestroyTextRenderComponent);

	DoorwayActorClass = ADoorway::StaticClass();
	
	// Initialize flags
	bIsGenerated = false;
}

bool ARoomActor::EnsureGeneratorReady()
{
	// Validate RoomData
	if (!RoomData)
	{ DebugHelpers->LogCritical(TEXT("RoomData is not assigned!")); return false; }

	if (RoomGridSize.X < 4 || RoomGridSize.Y < 4)
	{ DebugHelpers->LogCritical(TEXT("GridSize is too small (min 4x4)!")); return false; }

	// Create RoomGenerator if needed
	if (!RoomGenerator)
	{
		DebugHelpers->LogVerbose(TEXT("Creating RoomGenerator..."));
		RoomGenerator = NewObject<URoomGenerator>(this, TEXT("RoomGenerator"));

		if (!RoomGenerator)
		{ DebugHelpers->LogCritical(TEXT("Failed to create RoomGenerator!")); return false; }
	}

	// Initialize if needed
	if (!RoomGenerator->IsInitialized())
	{
		DebugHelpers->LogVerbose(TEXT("Initializing RoomGenerator..."));

		if (!RoomGenerator->Initialize(RoomData, RoomGridSize))
		{ DebugHelpers->LogCritical(TEXT("Failed to initialize RoomGenerator!")); return false; }

		DebugHelpers->LogVerbose(TEXT("Creating grid cells..."));
		RoomGenerator->CreateGrid();
	}

	return true;
}

#if WITH_EDITOR
#pragma region In Editor Functions
#pragma region Floor Generation
void ARoomActor::GenerateRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
	
	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
		return;
	}
	
	DebugHelpers->bShowGrid = true;
	DebugHelpers->bShowCellStates = true;
	DebugHelpers->bShowCoordinates = true;
	DebugHelpers->bShowForcedEmptyRegions = true;
	DebugHelpers->bShowForcedEmptyCells = true;
	
	// CREATE DEBUG VISUALIZATION (heavy, debug-only)
	DebugHelpers->LogImportant(TEXT("Creating debug visualization... "));
	
	UpdateVisualization();
	
	// Set flag for external checks
	bIsGenerated = true;
	
	LogRoomStatistics();
	
	DebugHelpers->LogImportant(TEXT("Room grid generated successfully!"));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE ROOM GRID"));
}

void ARoomActor::ClearRoomGrid()
{
	DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));

	if (! RoomGenerator || !bIsGenerated)
	{
		DebugHelpers->LogImportant(TEXT("No room grid to clear. "));
		DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));
		return;
	}
	
	DebugHelpers->bShowGrid = false;
	DebugHelpers->bShowCellStates = false;
	DebugHelpers->bShowCoordinates = false;
	DebugHelpers->bShowForcedEmptyRegions = false;
	DebugHelpers->bShowForcedEmptyCells = false;
	
	// Clear floor meshes first
	ClearFloorMeshes();
	
	// Clear wall meshes
	ClearWallMeshes();
	
	// Clear corner meshes
	ClearCornerMeshes();
	
	//Clear doorway meshes
	ClearDoorwayMeshes();
	
	// ✅ NEW: Clear doorway LAYOUT (not just meshes)
	if (RoomGenerator)
	{
		RoomGenerator->ClearPlacedDoorways();
	}

	// Clear the grid
	RoomGenerator->ClearGrid();
	bIsGenerated = false;
	
	// Clear coordinate text components (DebugHelpers manages them)
	DebugHelpers->ClearCoordinateTextComponents();
	
	// Clear debug drawings
	DebugHelpers->ClearDebugDrawings();

	DebugHelpers->LogImportant(TEXT("Room grid cleared. "));
	DebugHelpers->LogSectionHeader(TEXT("CLEAR ROOM GRID"));
}

void ARoomActor::GenerateFloorMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
	
	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator! "));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	// CLEANUP: Clear existing floor meshes
	ClearFloorMeshes();
	
	// Generate Floor Layout
	DebugHelpers->LogImportant(TEXT("Generating floor layout..."));
	if (!RoomGenerator->GenerateFloor())
	{
		DebugHelpers->LogCritical(TEXT("Floor generation failed!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	DebugHelpers->LogImportant(TEXT("Analyzing room topology..."));
	RoomGenerator->AnalyzeTopology();
	
	// Temporary debug - see topology stats
	int32 BorderCount = RoomGenerator->GetBorderCells().Num();
	int32 CornerCount = RoomGenerator->GetCornerCells().Num();
	int32 CenterCount = RoomGenerator->GetCenterCells().Num();

	DebugHelpers->LogImportant(FString::Printf(TEXT("Topology Stats:   Border=%d, Corner=%d, Center=%d"),
	
		BorderCount, CornerCount, CenterCount));
	// SPAWNING: Get placed meshes from generator
	const TArray<FPlacedMeshInfo>& PlacedMeshes = RoomGenerator->GetPlacedFloorMeshes();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d floor mesh instances... "), PlacedMeshes.Num()));
	

	// SPAWNING: Create ISM components and add instances
	for (const FPlacedMeshInfo& PlacedMesh : PlacedMeshes)
	{
		// Get or create ISM component for this mesh
		UInstancedStaticMeshComponent* ISM = URoomSpawnerHelpers::GetOrCreateISMComponent(
			this,
			PlacedMesh.MeshInfo.MeshAsset,
			FloorMeshComponents,
			TEXT("FloorISM_"),
			true
		);

		if (ISM)
		{
			// ✅ CHANGED: Pass zero offset - instances are in local space relative to ISM component
			int32 InstanceIndex = URoomSpawnerHelpers::SpawnMeshInstance(
				ISM, 
				PlacedMesh.LocalTransform,  // Actually local transform (misnamed)
				FVector::ZeroVector         // No offset needed
			);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf(
					TEXT("  Spawned floor mesh at grid position (%d, %d), instance %d"),
					PlacedMesh.GridPosition. X, PlacedMesh. GridPosition.Y, InstanceIndex));
			}
			else
			{
				DebugHelpers->LogVerbose(FString::Printf(
					TEXT("  Failed to spawn floor mesh at grid position (%d, %d)"),
					PlacedMesh.GridPosition.X, PlacedMesh.GridPosition.Y));
			}
		}
	}
	
	DebugHelpers->LogImportant(FString::Printf(TEXT("Floor meshes generated:  %d instances across %d unique meshes"),
		PlacedMeshes.Num(), FloorMeshComponents.Num()));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
}

void ARoomActor::ClearFloorMeshes()
{
	// Clear all floor ISM components
	URoomSpawnerHelpers:: ClearISMComponentMap(FloorMeshComponents);

	// Clear generator data AND reset grid state
	if (RoomGenerator)
	{
		// Clear placed mesh array
		RoomGenerator->ClearPlacedFloorMeshes();
		
		// Reset grid cells (ECT_FloorMesh → back to target type)
		RoomGenerator->ResetGridCellStates();
	}
	DebugHelpers->LogImportant(TEXT("Floor meshes cleared"));
}
#pragma endregion

#pragma region Wall Generation
void ARoomActor::GenerateWallMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));

	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE FLOOR MESHES"));
		return;
	}
	
	// Clear existing wall meshes
	ClearWallMeshes();
	
	// Generate wall layout (logic only)
	DebugHelpers->LogImportant(TEXT("Generating wall layout..."));
	if (! RoomGenerator->GenerateWalls())
	{
		DebugHelpers->LogCritical(TEXT("Wall generation failed!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
		return;
	}
	
	// Get placed walls and spawn
	const TArray<FPlacedWallInfo>& PlacedWalls = RoomGenerator->GetPlacedWalls();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d wall segments...  "), PlacedWalls.Num()));
	
	// Spawn wall segments
	for (const FPlacedWallInfo& PlacedWall : PlacedWalls) 
	{ 
		SpawnWallSegment(PlacedWall, FVector:: ZeroVector);  // ✅ Pass zero offset
	}
	
	DebugHelpers->LogImportant(TEXT("Wall meshes generated successfully!"));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE WALL MESHES"));
}

void ARoomActor::SpawnWallSegment(const FPlacedWallInfo& PlacedWall, const FVector& RoomOrigin)
{
	// Delegate to helper
	URoomSpawnerHelpers::SpawnWallSegment(this, PlacedWall,WallMeshComponents, 
	RoomOrigin, TEXT("WallISM_"), DebugHelpers);
}

void ARoomActor::ClearWallMeshes()
{
	// Clear all wall ISM components
	URoomSpawnerHelpers::ClearISMComponentMap(WallMeshComponents);

	// Clear generator data
	if (RoomGenerator) { RoomGenerator->ClearPlacedWalls();	}
	DebugHelpers->LogImportant(TEXT("Wall meshes cleared"));
}
#pragma endregion

#pragma region Corner Generation
void ARoomActor::GenerateCornerMeshes()
{
	 DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));

    if (!EnsureGeneratorReady())
    {
        DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    // Clear existing corners
    ClearCornerMeshes();

    // Generate corner layout (logic only)
    DebugHelpers->LogImportant(TEXT("Generating corner layout..."));
    if (!RoomGenerator->GenerateCorners())
    {
        DebugHelpers->LogCritical(TEXT("Corner generation failed!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    // Get placed corners and spawn
    const TArray<FPlacedCornerInfo>& PlacedCorners = RoomGenerator->GetPlacedCorners();
    
    if (PlacedCorners.Num() == 0)
    {
        DebugHelpers->LogImportant(TEXT("No corners to spawn (no corner mesh assigned)"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
        return;
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d corner pieces..."), PlacedCorners.Num()));

    // Spawn corner meshes
    for (const FPlacedCornerInfo& PlacedCorner : PlacedCorners)
    {
        // Get or create ISM component for corner mesh
        UInstancedStaticMeshComponent* ISM = URoomSpawnerHelpers::GetOrCreateISMComponent(
            this,
            PlacedCorner. CornerMesh,
            CornerMeshComponents,
            TEXT("CornerISM_"),
            true
        );

        if (ISM)
        {
            int32 InstanceIndex = URoomSpawnerHelpers::SpawnMeshInstance(
                ISM,
                PlacedCorner.Transform,
                FVector::ZeroVector
            );

            if (InstanceIndex >= 0)
            {
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned %s corner (instance %d)"),
                    *UEnum::GetValueAsString(PlacedCorner.Corner), InstanceIndex));
            }
            else
            {
                DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn %s corner"),
                    *UEnum::GetValueAsString(PlacedCorner. Corner)));
            }
        }
    }

    DebugHelpers->LogImportant(TEXT("Corner meshes generated successfully!"));
    DebugHelpers->LogSectionHeader(TEXT("GENERATE CORNER MESHES"));
}

void ARoomActor::ClearCornerMeshes()
{
	// Clear all corner ISM components
	URoomSpawnerHelpers::ClearISMComponentMap(CornerMeshComponents);

	// Clear generator data
	if (RoomGenerator)
	{
		RoomGenerator->ClearPlacedCorners();
	}

	DebugHelpers->LogImportant(TEXT("Corner meshes cleared"));
}
#pragma endregion

#pragma region Doorway Generation
void ARoomActor::GenerateDoorwayMeshes()
{
    DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));

    if (! EnsureGeneratorReady())
    {
        DebugHelpers->LogCritical(TEXT("Failed to initialize generator! "));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    // Clear existing doorways
    ClearDoorwayMeshes();

    // ✅ CHANGED:   ALWAYS call GenerateDoorways() to recalculate transforms
    DebugHelpers->LogImportant(TEXT("Regenerating doorway transforms with current offsets... "));
    
    if (!RoomGenerator->GenerateDoorways())
    {
        DebugHelpers->LogCritical(TEXT("Doorway generation failed!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    // Get doorways (from cache or newly generated)
    const TArray<FPlacedDoorwayInfo>& FinalDoorways = RoomGenerator->GetPlacedDoorways();
    
    if (FinalDoorways. Num() == 0)
    {
        DebugHelpers->LogImportant(TEXT("No doorways to spawn (none configured)"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d doorway actors... "), FinalDoorways.Num()));

    // Validate doorway actor class
    if (! DoorwayActorClass)
    {
        DebugHelpers->LogCritical(TEXT("DoorwayActorClass is not set!"));
        DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
        return;
    }

    int32 DoorwaysSpawned = 0;
    int32 DoorwaysSkipped = 0;
	
	

    for (const FPlacedDoorwayInfo& PlacedDoor : FinalDoorways)
    {
        // Validate door data
        if (!PlacedDoor.DoorData)
        {
            DebugHelpers->LogVerbose(TEXT("  Doorway has null DoorData - skipping"));
            DoorwaysSkipped++;
            continue;
        }

        // Calculate world transform (room space → world space)
        FTransform LocalTransform  = PlacedDoor.FrameTransform;

        // Spawn parameters
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // Spawn doorway actor
        ADoorway* DoorwayActor = GetWorld()->SpawnActor<ADoorway>(
            DoorwayActorClass,
            LocalTransform,
            SpawnParams
        );

        if (DoorwayActor)
        {
        	DoorwayActor->AttachToActor(this, FAttachmentTransformRules:: KeepRelativeTransform);
        	
            // Initialize doorway with configuration
            DoorwayActor->InitializeDoorway(
                PlacedDoor.DoorData,
                PlacedDoor.Edge,
                PlacedDoor.bIsStandardDoorway
            );

            // Store reference
            SpawnedDoorwayActors.Add(DoorwayActor);
            DoorwaysSpawned++;

            FString DoorType = PlacedDoor.bIsStandardDoorway ? TEXT("Standard") : TEXT("Manual");
            DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned %s doorway on edge %s"),
                *DoorType, *UEnum::GetValueAsString(PlacedDoor.Edge)));
        }
        else
        {
            DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn doorway on edge %s"),
                *UEnum::GetValueAsString(PlacedDoor.Edge)));
            DoorwaysSkipped++;
        }
    }

    DebugHelpers->LogImportant(FString::Printf(TEXT("Doorway spawning complete:  %d actors spawned, %d skipped"),
        DoorwaysSpawned, DoorwaysSkipped));
    DebugHelpers->LogSectionHeader(TEXT("GENERATE DOORWAY MESHES"));
}

void ARoomActor::ClearDoorwayMeshes()
{
	// ✅ CHANGED:  Destroy spawned doorway actors instead of clearing ISM components
    
	for (ADoorway* DoorwayActor : SpawnedDoorwayActors)
	{
		if (IsValid(DoorwayActor))
		{
			DoorwayActor->Destroy();
		}
	}

	SpawnedDoorwayActors.Empty();
	
	// Layout is cached and persists until ClearRoomGrid()
	// Transforms will be recalculated with current offsets on next spawn

	DebugHelpers->LogImportant(TEXT("Doorway actors cleared (layout preserved, offsets will update on next spawn)"));
}

// Add these at the end of the file (or with your other generation functions):

void ARoomActor::GenerateCeilingMeshes()
{
	DebugHelpers->LogSectionHeader(TEXT("GENERATE CEILING MESHES"));
	
	if (!EnsureGeneratorReady())
	{
		DebugHelpers->LogCritical(TEXT("Failed to initialize generator!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE CEILING MESHES"));
		return;
	}
	
	// CLEANUP: Clear existing ceiling meshes
	ClearCeilingMeshes();
	
	// Generate Ceiling Layout
	DebugHelpers->LogImportant(TEXT("Generating ceiling layout... "));
	if (! RoomGenerator->GenerateCeiling())
	{
		DebugHelpers->LogCritical(TEXT("Ceiling generation failed!"));
		DebugHelpers->LogSectionHeader(TEXT("GENERATE CEILING MESHES"));
		return;
	}
	
	// SPAWNING: Get placed meshes from generator
	const TArray<FPlacedCeilingInfo>& PlacedMeshes = RoomGenerator->GetPlacedCeilingTiles();
	DebugHelpers->LogImportant(FString::Printf(TEXT("Spawning %d ceiling mesh instances... "), PlacedMeshes.Num()));
	
	// SPAWNING: Create ISM components and add instances
	for (const FPlacedCeilingInfo& PlacedMesh : PlacedMeshes)
	{
		// Get or create ISM component for this mesh
		UInstancedStaticMeshComponent* ISM = URoomSpawnerHelpers::GetOrCreateISMComponent(this,
		PlacedMesh.MeshInfo.MeshAsset, CeilingMeshComponents, TEXT("CeilingISM_"), true);

		if (ISM)
		{
			int32 InstanceIndex = URoomSpawnerHelpers:: SpawnMeshInstance(ISM, PlacedMesh.LocalTransform, FVector::ZeroVector);

			if (InstanceIndex >= 0)
			{
				DebugHelpers->LogVerbose(FString::Printf(TEXT("  Spawned ceiling mesh at grid position (%d, %d), instance %d"),
				PlacedMesh.GridCoordinate.X, PlacedMesh.GridCoordinate. Y, InstanceIndex));
			}
			else
			{
				DebugHelpers->LogVerbose(FString::Printf(TEXT("  Failed to spawn ceiling mesh at grid position (%d, %d)"),
				PlacedMesh.GridCoordinate. X, PlacedMesh. GridCoordinate.Y));
			}
		}
	}
	
	DebugHelpers->LogImportant(FString::Printf(TEXT("Ceiling meshes generated:  %d instances across %d unique meshes"),
	PlacedMeshes.Num(), CeilingMeshComponents.Num()));
	DebugHelpers->LogSectionHeader(TEXT("GENERATE CEILING MESHES"));
}

void ARoomActor::ClearCeilingMeshes()
{
	// Clear all ceiling ISM components
	URoomSpawnerHelpers::ClearISMComponentMap(CeilingMeshComponents);

	// Clear generator data
	if (RoomGenerator)
	{
		// Clear placed mesh array
		RoomGenerator->ClearPlacedCeiling();
	}
	
	DebugHelpers->LogImportant(TEXT("Ceiling meshes cleared"));
}

#pragma region Doorway Side Fill Spawning


#pragma endregion
#pragma endregion

void ARoomActor::RefreshVisualization()
{
	DebugHelpers->LogImportant(TEXT("Refreshing visualization..."));

	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize.  Generate a room first.")); return;
	}

	// Clear existing drawings
	DebugHelpers->ClearDebugDrawings();

	// Redraw with current settings
	UpdateVisualization();

	DebugHelpers->LogImportant(TEXT("Visualization refreshed. "));
}
#pragma endregion

#pragma region Debug Functions
void ARoomActor::ToggleCoordinates()
{
	DebugHelpers->bShowCoordinates = !DebugHelpers->bShowCoordinates;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Coordinates display: %s"), 
		DebugHelpers->bShowCoordinates ? TEXT("ON") : TEXT("OFF")));
    
	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize. Generate a room first. "));
		return;
	}
    
	// ========================================================================
	// Coordinates use UTextRenderComponent (not debug shapes)
	// They're managed separately and don't require ClearDebugDrawings()
	// Just call the coordinate function directly
	// ========================================================================
    
	// Get grid data
	FVector RoomOrigin = GetActorLocation();
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	float CellSize = RoomGenerator->GetCellSize();
    
	// Toggle coordinates (function handles clearing internally)
	DebugHelpers->DrawGridCoordinatesWithTextComponents(GridSize, CellSize, RoomOrigin);
}

void ARoomActor:: ToggleGrid()
{
	DebugHelpers->bShowGrid = !DebugHelpers->bShowGrid;
	DebugHelpers->LogImportant(FString::Printf(TEXT("Grid outline display: %s"), 
		DebugHelpers->bShowGrid ? TEXT("ON") : TEXT("OFF")));
	
	RefreshVisualization();
}

void ARoomActor:: ToggleCellStates()
{
	  
	// Toggle cell state visualization
	DebugHelpers->bShowCellStates = ! DebugHelpers->bShowCellStates;
	DebugHelpers->bShowForcedEmptyRegions = DebugHelpers->bShowCellStates;
	DebugHelpers->bShowForcedEmptyCells = DebugHelpers->bShowCellStates;
    
	// ✅ NEW: Also toggle grid lines with cell states
	// Grid provides context for understanding cell visualization
	DebugHelpers->bShowGrid = DebugHelpers->bShowCellStates;
    
	DebugHelpers->LogImportant(FString::Printf(TEXT("Cell states display: %s"), 
		DebugHelpers->bShowCellStates ?  TEXT("ON") : TEXT("OFF")));
    
	if (!bIsGenerated || !RoomGenerator)
	{
		DebugHelpers->LogImportant(TEXT("No room to visualize. Generate a room first."));
		return;
	}
    
	// Use standard refresh (clear and redraw everything based on toggle states)
	RefreshVisualization();
}

UTextRenderComponent* ARoomActor::CreateTextRenderComponent(FVector WorldPosition, FString Text, FColor Color, float Scale)
{
	// Create new text render component
	UTextRenderComponent* TextComp = NewObject<UTextRenderComponent>(this);
	
	if (! TextComp) return nullptr;

	// Register and attach component
	TextComp->RegisterComponent();
	TextComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);

	// Set text properties
	TextComp->SetText(FText::FromString(Text));
	TextComp->SetWorldSize(Scale * 10.0f); // Scale for visibility
	TextComp->SetTextRenderColor(Color);
	TextComp->SetHorizontalAlignment(EHTA_Center);
	TextComp->SetVerticalAlignment(EVRTA_TextCenter);
	
	// Set world location
	TextComp->SetWorldLocation(WorldPosition);
	
	// Rotate to face upward (readable from above in editor)
	TextComp->SetWorldRotation(FRotator(45.0f, 180.0f, 0.0f));

	// Make visible in editor
	TextComp->SetVisibility(true);
	TextComp->SetHiddenInGame(true); // Show in PIE too
	
	return TextComp;
}

void ARoomActor::DestroyTextRenderComponent(UTextRenderComponent* TextComp)
{
	if (!TextComp || !TextComp->IsValidLowLevel()) return;

	// Destroy the component
	TextComp->DestroyComponent();
}

void ARoomActor::LogRoomStatistics()
{
	if (!RoomGenerator) return;

	DebugHelpers->LogSectionHeader(TEXT("ROOM STATISTICS"));
	
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	int32 TotalCells = RoomGenerator->GetTotalCellCount();
	int32 EmptyCells = RoomGenerator->GetCellCountByType(EGridCellType::ECT_Empty);
	int32 OccupiedCells = RoomGenerator->GetCellCountByType(EGridCellType::ECT_FloorMesh);
	float OccupancyPercent = RoomGenerator->GetOccupancyPercentage();

	DebugHelpers->LogStatistic(TEXT("Grid Size"), FString::Printf(TEXT("%d x %d"), GridSize.X, GridSize.Y));
	DebugHelpers->LogStatistic(TEXT("Total Cells"), TotalCells);
	DebugHelpers->LogStatistic(TEXT("Empty Cells"), EmptyCells);
	DebugHelpers->LogStatistic(TEXT("Occupied Cells"), OccupiedCells);
	DebugHelpers->LogStatistic(TEXT("Occupancy"), OccupancyPercent);
	
	DebugHelpers->LogSectionHeader(TEXT("ROOM STATISTICS"));
}

void ARoomActor::LogFloorStatistics()
{
	if (!RoomGenerator) return;

	DebugHelpers->LogSectionHeader(TEXT("FLOOR STATISTICS"));

	int32 LargeTiles, MediumTiles, SmallTiles, FillerTiles;
	RoomGenerator->GetFloorStatistics(LargeTiles, MediumTiles, SmallTiles, FillerTiles);

	int32 TotalTiles = LargeTiles + MediumTiles + SmallTiles + FillerTiles;
	float Coverage = RoomGenerator->GetOccupancyPercentage();
	int32 EmptyCells = RoomGenerator->GetCellCountByType(EGridCellType:: ECT_Empty);

	DebugHelpers->LogStatistic(TEXT("Large Tiles (400x400)"), LargeTiles);
	DebugHelpers->LogStatistic(TEXT("Medium Tiles (200x200)"), MediumTiles);
	DebugHelpers->LogStatistic(TEXT("Small Tiles (100x)"), SmallTiles);
	DebugHelpers->LogStatistic(TEXT("Filler Tiles"), FillerTiles);
	DebugHelpers->LogStatistic(TEXT("Total Tiles Placed"), TotalTiles);
	DebugHelpers->LogStatistic(TEXT("Floor Coverage"), Coverage);
	DebugHelpers->LogStatistic(TEXT("Empty Cells Remaining"), EmptyCells);

	DebugHelpers->LogSectionHeader(TEXT("FLOOR STATISTICS"));
}

void ARoomActor::UpdateVisualization()
{
	if (!RoomGenerator) return;

	// Get grid data
	FVector RoomOrigin = GetActorLocation();
	FIntPoint GridSize = RoomGenerator->GetGridSize();
	float CellSize = RoomGenerator->GetCellSize();
	const TArray<EGridCellType>& GridState = RoomGenerator->GetGridState();
	DebugHelpers->DrawGrid(GridSize, GridState, CellSize, RoomOrigin);

	// Draw forced empty regions (if any)
	if (RoomData && RoomData->ForcedEmptyRegions.Num() > 0)
	{ DebugHelpers->DrawForcedEmptyRegions(RoomData->ForcedEmptyRegions, GridSize, CellSize, RoomOrigin); }

	// Draw forced empty cells (if any)
	if (RoomData && RoomData->ForcedEmptyFloorCells.Num() > 0)
	{ DebugHelpers->DrawForcedEmptyCells(RoomData->ForcedEmptyFloorCells, GridSize, CellSize, RoomOrigin);}
	
	// Draw wall indicators (if topology analyzed and enabled)
	if (RoomGenerator->IsTopologyAnalyzed() && DebugHelpers->bShowWallDirections)
	{
		const TMap<FIntPoint, FCellData>& CellMetadata = RoomGenerator->GetCellMetadata();
		DebugHelpers->DrawWallIndicators(CellMetadata, CellSize, RoomOrigin);
	}
	
	DebugHelpers->LogVerbose(TEXT("Visualization updated."));
}
#pragma endregion
#endif // WITH_EDITOR

#pragma region Topology Analysis functions
void URoomGenerator::AnalyzeTopology()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("URoomGenerator::AnalyzeTopology - Generator not initialized"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::AnalyzeTopology - Starting topology analysis..."));

	// Clear existing metadata
	CellMetadata.Empty();

	int32 CellsAnalyzed = 0;

	// Analyze each occupied cell
	for (int32 Y = 0; Y < GridSize.Y; ++Y)
	{
		for (int32 X = 0; X < GridSize.X; ++X)
		{
			FIntPoint Cell(X, Y);
			int32 Index = Y * GridSize.X + X;

			// Only analyze occupied cells (has floor mesh or custom)
			if (GridState[Index] == EGridCellType::ECT_FloorMesh || 
			    GridState[Index] == EGridCellType::ECT_Custom)
			{
				// Create cell data
				FCellData CellData(Cell);
				CellData.bIsOccupied = true;

				// Count neighbors
				int32 NeighborCount = CountOccupiedNeighbors(Cell);

				// Detect walls
				DetectWalls(Cell, CellData);

				// Classify zone
				CellData.CellZone = ClassifyCellZone(NeighborCount, CellData.WallDirections);

				// Store in metadata map
				CellMetadata.Add(Cell, CellData);
				CellsAnalyzed++;
			}
		}
	}

	bTopologyAnalyzed = true;

	UE_LOG(LogTemp, Log, TEXT("URoomGenerator::AnalyzeTopology - Analyzed %d cells"), CellsAnalyzed);
}

int32 URoomGenerator::CountOccupiedNeighbors(FIntPoint Cell) const
{
	int32 Count = 0;

	// Check all 4 cardinal directions
	TArray<ECellDirection> Directions = { 
		ECellDirection::North, 
		ECellDirection::East, 
		ECellDirection::South, 
		ECellDirection:: West 
	};

	for (ECellDirection Direction : Directions)
	{
		FIntPoint Neighbor = GetNeighborCell(Cell, Direction);

		// Check if neighbor is in bounds
		if (Neighbor.X >= 0 && Neighbor. X < GridSize.X && 
		    Neighbor.Y >= 0 && Neighbor.Y < GridSize.Y)
		{
			int32 Index = Neighbor.Y * GridSize.X + Neighbor.X;

			// Check if occupied
			if (GridState[Index] == EGridCellType:: ECT_FloorMesh || 
			    GridState[Index] == EGridCellType:: ECT_Custom)
			{
				Count++;
			}
		}
	}

	return Count;
}

void URoomGenerator::DetectWalls(FIntPoint Cell, FCellData& OutCellData)
{
	// Check all 4 cardinal directions
	TArray<ECellDirection> Directions = { 
		ECellDirection::North, 
		ECellDirection::East, 
		ECellDirection::South, 
		ECellDirection:: West 
	};

	for (ECellDirection Direction : Directions)
	{
		FIntPoint Neighbor = GetNeighborCell(Cell, Direction);

		// Wall exists if: 
		// 1. Neighbor is out of bounds, OR
		// 2. Neighbor is empty/void
		bool bIsWall = false;

		if (Neighbor.X < 0 || Neighbor.X >= GridSize.X || 
		    Neighbor.Y < 0 || Neighbor.Y >= GridSize.Y)
		{
			// Out of bounds = wall
			bIsWall = true;
		}
		else
		{
			int32 Index = Neighbor.Y * GridSize.X + Neighbor.X;

			// Check if neighbor is empty or void
			if (GridState[Index] == EGridCellType::ECT_Empty || 
			    GridState[Index] == EGridCellType::ECT_Void)
			{
				bIsWall = true;
			}
		}

		if (bIsWall)
		{
			OutCellData. WallDirections.Add(Direction);
		}
	}
}

ECellZone URoomGenerator::ClassifyCellZone(int32 NeighborCount, const TSet<ECellDirection>& WallDirections) const
{
	int32 WallCount = WallDirections.Num();

	// Dead-end (3 walls)
	if (WallCount == 3)
	{
		return ECellZone::DeadEnd;
	}

	// Corner (2 walls)
	if (WallCount == 2)
	{
		// Check if walls are adjacent (90° corner) or opposite (corridor)
		TArray<ECellDirection> Walls = WallDirections. Array();
		
		if (AreDirectionsAdjacent(Walls[0], Walls[1]))
		{
			// Adjacent walls = corner
			// TODO: Distinguish internal vs external corners (needs more context)
			return ECellZone::Corner;
		}
		else
		{
			// Opposite walls = corridor/border
			return ECellZone:: Border;
		}
	}

	// Border (1 wall)
	if (WallCount == 1)
	{
		return ECellZone::Border;
	}

	// Center (0 walls)
	if (WallCount == 0)
	{
		return ECellZone::Center;
	}

	// Default
	return ECellZone::Center;
}

FIntPoint URoomGenerator::GetNeighborCell(FIntPoint Cell, ECellDirection Direction) const
{
	switch (Direction)
	{
		case ECellDirection::North: 
			return FIntPoint(Cell. X, Cell.Y + 1);
		case ECellDirection::East:
			return FIntPoint(Cell.X + 1, Cell.Y);
		case ECellDirection::South: 
			return FIntPoint(Cell. X, Cell.Y - 1);
		case ECellDirection:: West:
			return FIntPoint(Cell.X - 1, Cell.Y);
		default:
			return Cell;
	}
}

bool URoomGenerator::AreDirectionsAdjacent(ECellDirection Dir1, ECellDirection Dir2) const
{
	// Adjacent pairs: N-E, E-S, S-W, W-N
	if ((Dir1 == ECellDirection:: North && Dir2 == ECellDirection::East) ||
	    (Dir1 == ECellDirection::East && Dir2 == ECellDirection::North))
		return true;

	if ((Dir1 == ECellDirection::East && Dir2 == ECellDirection::South) ||
	    (Dir1 == ECellDirection:: South && Dir2 == ECellDirection::East))
		return true;

	if ((Dir1 == ECellDirection::South && Dir2 == ECellDirection::West) ||
	    (Dir1 == ECellDirection::West && Dir2 == ECellDirection:: South))
		return true;

	if ((Dir1 == ECellDirection::West && Dir2 == ECellDirection::North) ||
	    (Dir1 == ECellDirection::North && Dir2 == ECellDirection::West))
		return true;

	return false;
}

//========================================================================
// TOPOLOGY QUERY FUNCTIONS
//========================================================================

TArray<FIntPoint> URoomGenerator:: GetCellsByZone(ECellZone Zone) const
{
	TArray<FIntPoint> Results;

	for (const auto& Pair : CellMetadata)
	{
		if (Pair.Value.CellZone == Zone)
		{
			Results.Add(Pair.Key);
		}
	}

	return Results;
}

TArray<FIntPoint> URoomGenerator::GetBorderCells() const
{
	TArray<FIntPoint> Results;

	for (const auto& Pair : CellMetadata)
	{
		if (Pair.Value. IsBorder())
		{
			Results.Add(Pair.Key);
		}
	}

	return Results;
}

TArray<FIntPoint> URoomGenerator::GetCornerCells() const
{
	return GetCellsByZone(ECellZone::Corner);
}

TArray<FIntPoint> URoomGenerator::GetCenterCells() const
{
	return GetCellsByZone(ECellZone::Center);
}
#pragma endregion