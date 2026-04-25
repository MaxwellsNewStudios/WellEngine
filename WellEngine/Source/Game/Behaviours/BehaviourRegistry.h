#pragma once

#include <string>
#include <map>
#include <functional>

namespace WellEngine
{	
	class Behaviour;
}

namespace WellEngine::BehaviourRegistry
{
	[[nodiscard]] const std::map<std::string, std::function<Behaviour *(void)>> &Get();

#ifdef DEBUG_BUILD
	[[nodiscard]] const std::map<std::string, std::string> &GetCategories();

	struct CategoryTree
	{
		struct CategoryNode
		{
			std::map<std::string, CategoryNode> subcategories;
			std::vector<std::string> behaviours;
		} root;
	};

	[[nodiscard]] const static inline CategoryTree &GetCategoryTree()
	{
		static CategoryTree tree;
		static bool generated = false;

		if (!generated)
		{
			auto &categories = GetCategories();

			for (const auto &[behaviourName, categoryPath] : categories)
			{
				CategoryTree::CategoryNode *currentNode = &tree.root;

				std::istringstream pathStream(categoryPath);
				std::string segment;

				while (std::getline(pathStream, segment, '/'))
				{
					if (segment.empty())
						continue;
					
					currentNode = &currentNode->subcategories[segment];
				}

				currentNode->behaviours.push_back(behaviourName);
			}

			generated = true;
		}

		return tree;
	}
#endif
}