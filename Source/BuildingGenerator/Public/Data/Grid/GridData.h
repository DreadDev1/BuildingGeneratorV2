// GridData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "GridData.generated.h"

// --- CORE CONSTANT DEFINITION ---
static constexpr float CELL_SIZE = 100.0f;

/* Cardinal directions for cell neighbors and walls */
UENUM(BlueprintType)
enum class ECellDirection : uint8
{
	North UMETA(DisplayName = "North"),
	East UMETA(DisplayName = "East"),
	South UMETA(DisplayName = "South"),
	West UMETA(DisplayName = "West")
};

/* Cell zone classification based on topology analysis Used for zone-based content spawning and queries */
UENUM(BlueprintType)
enum class ECellZone : uint8
{
	/** Cell is empty/unoccupied */
	Empty UMETA(DisplayName = "Empty"),
	
	/** Cell is in the center of the room (4 neighbors) */
	Center UMETA(DisplayName = "Center"),
	
	/** Cell is on the border (3 neighbors) */
	Border UMETA(DisplayName = "Border"),
	
	/** Cell is a corner (2 adjacent neighbors) */
	Corner UMETA(DisplayName = "Corner"),
	
	/** Cell is an external corner (convex corner) */
	ExternalCorner UMETA(DisplayName = "External Corner"),
	
	/** Cell is an internal corner (concave corner) */
	InternalCorner UMETA(DisplayName = "Internal Corner"),
	
	/** Cell is a doorway/connection point */
	Door UMETA(DisplayName = "Door"),
	
	/** Cell is a dead-end (1 neighbor only) */
	DeadEnd UMETA(DisplayName = "Dead End")
};

// Defines the content type of a 100cm grid cell
UENUM(BlueprintType)
enum class EGridCellType : uint8
{
	ECT_Empty 		UMETA(DisplayName = "Empty"),
	ECT_FloorMesh 	UMETA(DisplayName = "Floor Mesh"),
	ECT_WallMesh 	UMETA(DisplayName = "Wall Boundary"),
	ECT_Doorway 	UMETA(DisplayName = "Doorway Cell"),
	
	ECT_Reserved    UMETA(DisplayName = "Reserved Cell"),
	ECT_Custom      UMETA(DisplayName = "Custom Cell"),
	ECT_Void        UMETA(DisplayName = "Void Cell"),
};

//========================================================================
// CELL DATA STRUCTURE (Phase 2 - Rich Topology Data)
//========================================================================

/**
 * FCellData - Rich cell metadata for topology analysis
 * 
 * Purpose: 
 *   - Supplements TArray<EGridCellType> GridState with detailed topology info
 *   - Enables zone-based queries (GetBorderCells, GetCornerCells, etc.)
 *   - Enables zone-based content spawning (spawn torches only on borders)
 * 
 * Usage:
 *   - Stored in TMap<FIntPoint, FCellData> CellMetadata in URoomGenerator
 *   - Populated by AnalyzeTopology() after grid creation
 *   - Queried by spawning systems for zone-based placement
 * 
 * Relationship to GridState:
 *   - GridState (TArray) = flat occupancy data (Empty, Custom, FloorMesh, etc.)
 *   - CellMetadata (TMap) = rich topology data (zone, walls, neighbors)
 *   - Both coexist - GridState for generation, CellMetadata for queries
 */
USTRUCT(BlueprintType)
struct BUILDINGGENERATOR_API FCellData
{
	GENERATED_BODY()

	//========================================================================
	// CORE DATA
	//========================================================================

	/** Grid coordinates (X, Y) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell Data")
	FIntPoint Coordinates;

	/** Cell zone classification (Center, Border, Corner, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell Data")
	ECellZone CellZone;

	/** Is this cell occupied? (true if part of room) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell Data")
	bool bIsOccupied;

	//========================================================================
	// TOPOLOGY DATA
	//========================================================================

	/** Directions that have walls (no neighbor in that direction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell Data|Topology")
	TSet<ECellDirection> WallDirections;

	/** Directions that have openings (doors/connections to other rooms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cell Data|Topology")
	TSet<ECellDirection> OpenDirections;

	//========================================================================
	// CONSTRUCTORS
	//========================================================================

	/** Default constructor */
	FCellData()
		: Coordinates(FIntPoint::ZeroValue)
		, CellZone(ECellZone::Empty)
		, bIsOccupied(false)
	{
	}

	/** Constructor with coordinates */
	FCellData(const FIntPoint& InCoords)
		: Coordinates(InCoords)
		, CellZone(ECellZone:: Center)
		, bIsOccupied(true)
	{
	}

	//========================================================================
	// QUERY FUNCTIONS
	//========================================================================

	/**
	 * Check if this cell has a wall in a given direction
	 * @param Direction - Direction to check (North, East, South, West)
	 * @return True if wall exists in that direction
	 */
	FORCEINLINE bool HasWallInDirection(ECellDirection Direction) const
	{
		return WallDirections. Contains(Direction);
	}

	/**
	 * Check if this cell has an opening in a given direction
	 * @param Direction - Direction to check
	 * @return True if opening (door/connection) exists in that direction
	 */
	FORCEINLINE bool HasOpeningInDirection(ECellDirection Direction) const
	{
		return OpenDirections.Contains(Direction);
	}

	/**
	 * Get number of walls
	 */
	FORCEINLINE int32 GetWallCount() const
	{
		return WallDirections. Num();
	}

	/**
	 * Get number of openings
	 */
	FORCEINLINE int32 GetOpeningCount() const
	{
		return OpenDirections.Num();
	}

	/**
	 * Get number of neighbors (cells adjacent in 4 directions without walls)
	 */
	FORCEINLINE int32 GetNeighborCount() const
	{
		// 4 possible neighbors - wall count = actual neighbors
		return 4 - WallDirections. Num();
	}

	/**
	 * Check if this cell is a border cell (has at least one wall)
	 */
	FORCEINLINE bool IsBorder() const
	{
		return WallDirections.Num() > 0;
	}

	/**
	 * Check if this cell is a corner (has exactly 2 walls)
	 */
	FORCEINLINE bool IsCorner() const
	{
		return WallDirections.Num() == 2;
	}

	/**
	 * Check if this cell is a dead-end (has 3 walls)
	 */
	FORCEINLINE bool IsDeadEnd() const
	{
		return WallDirections. Num() == 3;
	}

	/**
	 * Check if cell is in center (no walls)
	 */
	FORCEINLINE bool IsCenter() const
	{
		return WallDirections.Num() == 0;
	}
};

//========================================================================
// CONNECTION POINT DATA (Room-to-Room Connections - Phase 5)
//========================================================================

/**
 * FRoomConnectionPoint - Data for connecting rooms via doorways
 * Used by multi-room system (Phase 5)
 */
USTRUCT(BlueprintType)
struct BUILDINGGENERATOR_API FRoomConnectionPoint
{
	GENERATED_BODY()

	/** Cell location of the connection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	FIntPoint CellLocation;

	/** Direction the connection faces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	ECellDirection Direction;

	/** Connected room reference (if any) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	AActor* ConnectedRoom;

	/** Default constructor */
	FRoomConnectionPoint()
		: CellLocation(FIntPoint::ZeroValue)
		, Direction(ECellDirection::North)
		, ConnectedRoom(nullptr)
	{
	}
};









