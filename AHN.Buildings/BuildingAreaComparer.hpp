#include <map>
#include <set>

#include <gdal_priv.h>

#include <CloudTools.DEM/DatasetTransformation.hpp>

namespace AHN
{
namespace Buildings
{
/// <summary>
/// Represents 
/// </summary>

class BuildingAreaComparer : public CloudTools::DEM::DatasetTransformation<int, uint32_t>
{
public:
	/// <summary>
	/// Initializes 
	/// </summary>
	/// <param name="ahn2Dataset">The AHN-2 dataset of the comparison.</param>
	/// <param name="ahn3Dataset">The AHN-3 dataset of the comparison.</param>
	/// <param name="ahn2Surface">The AHN-2 surface data of the comparison.</param>
	/// <param name="ahn3Surface">The AHN-3 surface data of the comparison.</param>
	/// <param name="targetPath">The target path of the comparison.</param>
	/// <param name="progress">he callback method to report progress.</param>
	BuildingAreaComparer(GDALDataset* ahn2Dataset, GDALDataset* ahn3Dataset,
						 GDALDataset* ahn2Surface, GDALDataset* ahn3Surface,
						 const std::string& targetPath,
						 CloudTools::Operation::ProgressType progress = nullptr)
		: CloudTools::DEM::DatasetTransformation<int, uint32_t>(
			std::vector<GDALDataset*>{ ahn2Dataset, ahn3Dataset, ahn2Surface, ahn3Surface }, targetPath, nullptr, progress)
	{
		initialize();
	}


	BuildingAreaComparer(const BuildingAreaComparer&) = delete;
	BuildingAreaComparer& operator=(const BuildingAreaComparer&) = delete;

private:
	/// <summary>
	/// Initializes the new instance of the class.
	/// </summary>
	void initialize();
};

void BuildingAreaComparer::initialize()
{
	this->nodataValue = 0;

	this->computation = [this](int sizeX, int sizeY)
	{
		std::map<std::pair<uint32_t, uint32_t>, std::pair<int, float>> stat;
		std::map<uint32_t, std::set<uint32_t> > pairings2;
		std::map<uint32_t, std::set<uint32_t> > pairings3;

		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				uint32_t ahn2 = this->sourceData(0, i, j);
				uint32_t ahn3 = this->sourceData(1, i, j);
				float ahn2H = this->sourceData(2, i, j);
				float ahn3H = this->sourceData(3, i, j);
				auto pair = std::make_pair(ahn2, ahn3);
				/*try
				{
					stat.at(pair);;
				}
				catch (std::exception e)
				{
					stat[pair].first = 0;
					stat[pair].second = 0;

				}*/
				if ((ahn2 > 0 || ahn3 > 0) && ahn2H < 10000 && ahn3H < 10000)
				{
					auto tmp1 = stat[pair].second * stat[pair].first;
					auto tmp2 = tmp1 + ahn3H - ahn2H;
					auto tmp3 = tmp2 / (stat[pair].first + 1);
					stat[pair].second = tmp3;
					// stat[pair].second = (stat[pair].second * stat[pair].first + (ahn3H - ahn2H)) / (stat[pair].first + 1);
				}
				stat[pair].first++;
				if (ahn2 > 0) pairings2[ahn2].insert(ahn3);
				if (ahn3 > 0) pairings3[ahn3].insert(ahn2);
			}

		std::set<std::pair<uint32_t, uint32_t> > type2;
		std::set<std::pair<uint32_t, uint32_t> > type3;
		std::set<std::pair<uint32_t, uint32_t> > type5;
		std::set<std::pair<uint32_t, uint32_t> > type8;
		std::set<std::pair<uint32_t, uint32_t> > type9;
		//std::set<std::pair<uint32_t, uint32_t> > type;

		for (auto const &entry : stat)
		{
			uint32_t id2 = entry.first.first;
			uint32_t id3 = entry.first.second;
			if (id2 > 0 || id3 > 0)
			{
				for (uint32_t id : pairings2[id2])
				{
					auto pair = std::make_pair(id2, id);
					const auto &data = stat[pair];
					if (id > 0)
					{
						if (abs(data.second) <= 2) type5.insert(pair);
						else type8.insert(pair);
					}
					else if (abs(data.second) >= 2) type2.insert(pair);
					else if (abs(data.second) <= 2) type9.insert(pair);
				}
				for (uint32_t id : pairings3[id3])
				{
					auto pair = std::make_pair(id, id3);
					const auto &data = stat[pair];
					if (id > 0)
					{
						if (abs(data.second) <= 2) type5.insert(pair);
						 else type8.insert(pair);
					}
					else if (abs(data.second) >= 2) type3.insert(pair);
					else if (abs(data.second) <= 2) type9.insert(pair);
				}
			}
		}

		for (int i = 0; i < sizeX; ++i)
			for (int j = 0; j < sizeY; ++j)
			{
				uint32_t ahn2 = this->sourceData(0, i, j);
				uint32_t ahn3 = this->sourceData(1, i, j);

				std::pair<uint32_t, uint32_t> p(ahn2, ahn3);
				bool significant = stat[p].first > 150;
				int result = this->nodataValue;
				if (significant)
				{
					if (type2.find(p) != type2.end()) result = 2;
					else if (type3.find(p) != type3.end()) result = 3;
					else if (type5.find(p) != type5.end()) result = 5;
					else if (type8.find(p) != type8.end()) result = 8;
					else if (type9.find(p) != type9.end()) result = 9;
				}
				this->setTargetData(i, j, result);
			}
	};
}

} // Buildings
} // AHN