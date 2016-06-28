#pragma once

namespace AHN
{
/// <summary>
/// Represents an administrative region.
/// </summary>
struct Region
{	
	/// <summary>
	/// The identifier of the region.
	/// </summary>
	int id;	
	/// <summary>
	/// The cumulative altimetry gained.
	/// </summary>
	float altimetryGained;
	/// <summary>
	/// The cumulative altimetry lost.
	/// </summary>
	float altimetryLost;
	/// <summary>
	/// The cumulative altimetry moved (gained + lost).
	/// </summary>
	float altimetryMoved;
	/// <summary>
	/// The cumulative altimetry change (gained - lost).
	/// </summary>
	float altimetryChange;
};
} // AHN
