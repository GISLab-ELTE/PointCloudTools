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
	float gained;
	/// <summary>
	/// The cumulative altimetry lost.
	/// </summary>
	float lost;
	/// <summary>
	/// The cumulative altimetry moved (gained + lost).
	/// </summary>
	float moved;
	/// <summary>
	/// The cumulative altimetry difference (gained - lost).
	/// </summary>
	float difference;
};
} // AHN
