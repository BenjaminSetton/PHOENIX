
#include <algorithm>
#include <cstdio>
#include <sstream>

#include <BSL/file_io.h>
#include <BSL/logger.h>
#include <BSL/math.h>

#include "core/global_settings.h"
#include "utils/cache_utils.h"
#include "utils/render_graph_utils.h"

using namespace BSL;

namespace PHX
{
	// Visualization color palette
	// Base hues
	static constexpr const char* HUE_BLUE   = "#2E86C1";
	static constexpr const char* HUE_GREEN  = "#239B56";
	static constexpr const char* HUE_ORANGE = "#CA6F1E";
	static constexpr const char* HUE_PURPLE = "#7D3C98";
	static constexpr const char* HUE_TEAL   = "#117864";
	static constexpr const char* HUE_RED    = "#C0392B";
	static constexpr const char* HUE_GREY   = "#656565";

	// Shade offsets
	static constexpr int SHADE_DARK   = -0x000032;  // resource fills (darkest)
	static constexpr int SHADE_BRIGHT =  0x00001E;  // pass fills / edges
	static constexpr int SHADE_LIGHT  =  0x00003C;  // edge labels (lightest)

	// Chrome
	static constexpr const char* COLOR_BG             = "#2B2B2B";
	static constexpr const char* COLOR_TEXT_PRIMARY   = "#FFFFFF";
	static constexpr const char* COLOR_TEXT_SECONDARY = "#BDC3C7";
	static constexpr const char* COLOR_PANEL_FILL     = "#3C3C3C";
	static constexpr const char* COLOR_BORDER_DEFAULT = "#BDC3C7";
	static constexpr const char* COLOR_BORDER_DIM     = "#5D6D7E";
	static constexpr const char* COLOR_SUBMISSION     = "#909497";

	// Hazard flags. RAW is a true data dependency, WAR/WAW are false dependencies caused by resource reuse
	static constexpr u8 HAZARD_RAW = 1 << 0;
	static constexpr u8 HAZARD_WAR = 1 << 1;
	static constexpr u8 HAZARD_WAW = 1 << 2;

	struct VizResourceStyle
	{
		const char* hue;
		const char* tag;
		const char* defaultName;
	};

	//--------------------------------------------------------------------------------------------

	bool IsWriteAccess(RESOURCE_ACCESS access)
	{
		return ((access == RESOURCE_ACCESS::READ_WRITE) || (access == RESOURCE_ACCESS::WRITE_ONLY));
	}

	bool IsReadAccess(RESOURCE_ACCESS access)
	{
		return ((access == RESOURCE_ACCESS::READ_WRITE) || (access == RESOURCE_ACCESS::READ_ONLY));
	}

	ResourceID HashResource(Handle resource, const RESOURCE_TYPE& type)
	{
		size_t seed = 0;
		HashCombine(seed, resource.GetIndex());
		HashCombine(seed, type);

		return static_cast<ResourceID>(seed);
	}

	QUEUE_TYPE ConvertPassTypeToQueueType(PASS_TYPE passType)
	{
		switch (passType)
		{
		case PASS_TYPE::COMPUTE:      return QUEUE_TYPE::COMPUTE;
		case PASS_TYPE::GRAPHICS:     return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::TRANSFER:     return QUEUE_TYPE::TRANSFER;
		case PASS_TYPE::RAY_TRACING:  return QUEUE_TYPE::GRAPHICS;
		case PASS_TYPE::AS_BUILD:     return QUEUE_TYPE::GRAPHICS;
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to convert pass type to queue type!");
		return QUEUE_TYPE::GRAPHICS;
	}

	ATTACHMENT_TYPE CalculateAttachmentType(TextureHandle handle)
	{
		AspectTypeFlags aspectFlags = handle.GetAspectFlags();
		switch (aspectFlags)
		{
		case ASPECT_TYPE_FLAG_COLOR:                              return ATTACHMENT_TYPE::COLOR;
		case (ASPECT_TYPE_FLAG_DEPTH | ASPECT_TYPE_FLAG_STENCIL): return ATTACHMENT_TYPE::DEPTH_STENCIL;
		case ASPECT_TYPE_FLAG_DEPTH:                              return ATTACHMENT_TYPE::DEPTH;
		case ASPECT_TYPE_FLAG_STENCIL:                            return ATTACHMENT_TYPE::STENCIL;
		default:
		{
			break;
		}
		}

		LogError("Failed to calculate attachment type from texture's aspect flags. No valid combination was found for aspect flags %u", aspectFlags);
		return ATTACHMENT_TYPE::INVALID;
	}

	const char* PassTypeToString(PASS_TYPE passType)
	{
		switch (passType)
		{
		case PASS_TYPE::GRAPHICS:     return "GRAPHICS";
		case PASS_TYPE::COMPUTE:      return "COMPUTE";
		case PASS_TYPE::TRANSFER:     return "TRANSFER";
		case PASS_TYPE::RAY_TRACING:  return "RAY_TRACING";
		case PASS_TYPE::AS_BUILD:     return "AS_BUILD";
		default:
		{
			break;
		}
		}

		ASSERT_ALWAYS("Failed to convert pass type to string");
		return "UNKNOWN";
	}

	//--------------------------------------------------------------------------------------------
	// Render graph visualization
	//--------------------------------------------------------------------------------------------

	// Shift each RGB component of a #RRGGBB color by delta, clamped to 0-255
	static std::string ShiftColor(const char* hex, int delta)
	{
		int r = std::stoi(std::string(hex + 1, 2), nullptr, 16);
		int g = std::stoi(std::string(hex + 3, 2), nullptr, 16);
		int b = std::stoi(std::string(hex + 5, 2), nullptr, 16);
		char buf[8];
		std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", 
			Clamp(r + delta, 0, 255), 
			Clamp(g + delta, 0, 255), 
			Clamp(b + delta, 0, 255));
		return buf;
	}

	// Escapes text for HTML-like labels and their attributes
	static std::string EscapeHtml(const std::string& str)
	{
		std::string out;
		out.reserve(str.size());
		for (char c : str)
		{
			switch (c)
			{
			case '&': out += "&amp;";  break;
			case '<': out += "&lt;";   break;
			case '>': out += "&gt;";   break;
			case '"': out += "&quot;"; break;
			default:  out += c;        break;
			}
		}
		return out;
	}

	// Escapes text for quoted DOT strings
	static std::string EscapeDot(const std::string& str)
	{
		std::string out;
		out.reserve(str.size());
		for (char c : str)
		{
			if (c == '"' || c == '\\')
			{
				out += '\\';
			}
			out += c;
		}
		return out;
	}

	static const char* GetPassTypeHue(PASS_TYPE passType)
	{
		switch (passType)
		{
		case PASS_TYPE::GRAPHICS:    return HUE_BLUE;
		case PASS_TYPE::COMPUTE:     return HUE_GREEN;
		case PASS_TYPE::TRANSFER:    return HUE_ORANGE;
		case PASS_TYPE::RAY_TRACING: return HUE_PURPLE;
		case PASS_TYPE::AS_BUILD:    return HUE_TEAL;
		default:                     return HUE_GREY;
		}
	}

	static VizResourceStyle GetResourceStyle(const VizResource& resource)
	{
		if (resource.isPresent)
		{
			return { HUE_RED, "PRESENT", "Backbuffer" };
		}

		switch (resource.type)
		{
		case RESOURCE_TYPE::BUFFER:                 return { HUE_PURPLE, "BUFFER",                 "Buffer" };
		case RESOURCE_TYPE::UNIFORM:                return { HUE_ORANGE, "UNIFORM",                "Uniforms" };
		case RESOURCE_TYPE::ACCELERATION_STRUCTURE: return { HUE_TEAL,   "ACCELERATION STRUCTURE", "Acceleration Structure" };
		default:                                    return { HUE_BLUE,   "TEXTURE",                "Texture" };
		}
	}

	// Prefers the resource's name, falling back to "<Type> <resource index>"
	static std::string GetResourceDisplayName(const VizResource& resource, u32 resourceIndex)
	{
		if (!resource.name.empty())
		{
			return resource.name;
		}
		return std::string(GetResourceStyle(resource).defaultName) + " " + std::to_string(resourceIndex);
	}

	static const char* AccessToString(RESOURCE_ACCESS access)
	{
		switch (access)
		{
		case RESOURCE_ACCESS::READ_ONLY:  return "READ_ONLY";
		case RESOURCE_ACCESS::WRITE_ONLY: return "WRITE_ONLY";
		case RESOURCE_ACCESS::READ_WRITE: return "READ_WRITE";
		default:                          return "UNKNOWN";
		}
	}

	static const char* AttachmentTypeToString(ATTACHMENT_TYPE type)
	{
		switch (type)
		{
		case ATTACHMENT_TYPE::COLOR:         return "COLOR";
		case ATTACHMENT_TYPE::DEPTH:         return "DEPTH";
		case ATTACHMENT_TYPE::STENCIL:       return "STENCIL";
		case ATTACHMENT_TYPE::DEPTH_STENCIL: return "DEPTH_STENCIL";
		case ATTACHMENT_TYPE::RESOLVE:       return "RESOLVE";
		default:                             return "NONE";
		}
	}

	static const char* LoadOpToString(ATTACHMENT_LOAD_OP loadOp)
	{
		switch (loadOp)
		{
		case ATTACHMENT_LOAD_OP::LOAD:   return "LOAD";
		case ATTACHMENT_LOAD_OP::CLEAR:  return "CLEAR";
		case ATTACHMENT_LOAD_OP::IGNORE: return "IGNORE";
		default:                         return "-";
		}
	}

	static const char* StoreOpToString(ATTACHMENT_STORE_OP storeOp)
	{
		switch (storeOp)
		{
		case ATTACHMENT_STORE_OP::STORE:  return "STORE";
		case ATTACHMENT_STORE_OP::IGNORE: return "IGNORE";
		default:                          return "-";
		}
	}

	static std::string HazardsToString(u8 hazards)
	{
		std::string str;
		if (hazards & HAZARD_RAW) { str += "RAW"; }
		if (hazards & HAZARD_WAR) { str += (str.empty() ? "" : "/"); str += "WAR"; }
		if (hazards & HAZARD_WAW) { str += (str.empty() ? "" : "/"); str += "WAW"; }
		return str;
	}

	static bool WriteVisualizationFile(const std::string& fileName, const std::string& contents)
	{
		const std::string fullPath = std::string(GetSettings().cacheDirectory) + "/" + fileName;
		FileIO io;
		FileOpenData openData{};
		openData.mode = FileMode::Truncate;
		openData.binary = false;
		openData.shareable = false;
		if (!io.Open(fullPath.c_str(), openData))
		{
			LogError("Failed to generate render graph visualization. Could not open file \"%s\" for writing!", fullPath.c_str());
			return false;
		}

		io.Write(contents.c_str(), static_cast<u32>(contents.size()));
		return true;
	}

	STATUS_CODE CreateGraphVisualization(const VizGraph& graph, const char* fileName)
	{
		if (fileName == nullptr)
		{
			LogError("Failed to generate render graph visualization. File name is null!");
			return STATUS_CODE::ERR_API;
		}

		const u32 passCount = static_cast<u32>(graph.passes.size());
		const u32 resourceCount = static_cast<u32>(graph.resources.size());
		const u32 executedCount = static_cast<u32>(graph.submissionOrder.size());

		std::vector<u32> submissionSlots(passCount, U32_MAX);
		for (u32 i = 0; i < executedCount; i++)
		{
			submissionSlots[graph.submissionOrder[i]] = i;
		}

		auto isExecutedPass = [&](u32 passIndex)
		{
			return (submissionSlots[passIndex] != U32_MAX);
		};

		auto getPassHue = [&](u32 passIndex) -> const char*
		{
			return isExecutedPass(passIndex) ? GetPassTypeHue(graph.passes[passIndex].passType) : HUE_GREY;
		};

		// ---- Pass-to-pass hazard edges (transitive reduction over each resource's usage chain) ----
		// Only consecutive hazards are kept: last writer -> readers (RAW), readers -> next writer (WAR),
		// writer -> next writer with no readers in between (WAW). All other hazards are implied transitively
		struct HazardResource
		{
			u32 resourceIndex;
			u8 hazards;
			const VizUsage* pDstUsage;
		};

		struct PassEdge
		{
			u32 srcPass;
			u32 dstPass;
			u8 hazards;
			bool isCritical;
			std::vector<HazardResource> resources;
		};

		std::vector<PassEdge> edges;
		auto addHazard = [&](u32 resourceIndex, const VizUsage& srcUsage, const VizUsage& dstUsage, u8 hazard)
		{
			if (srcUsage.passIndex == dstUsage.passIndex)
			{
				return;
			}

			auto edgeIter = std::find_if(edges.begin(), edges.end(), [&](const PassEdge& edge)
			{
				return (edge.srcPass == srcUsage.passIndex && edge.dstPass == dstUsage.passIndex);
			});
			if (edgeIter == edges.end())
			{
				edges.push_back({ srcUsage.passIndex, dstUsage.passIndex, 0, false, {} });
				edgeIter = edges.end() - 1;
			}
			edgeIter->hazards = static_cast<u8>(edgeIter->hazards | hazard);

			auto resourceIter = std::find_if(edgeIter->resources.begin(), edgeIter->resources.end(), [&](const HazardResource& hazardResource)
			{
				return (hazardResource.resourceIndex == resourceIndex);
			});
			if (resourceIter == edgeIter->resources.end())
			{
				edgeIter->resources.push_back({ resourceIndex, hazard, &dstUsage });
			}
			else
			{
				resourceIter->hazards = static_cast<u8>(resourceIter->hazards | hazard);
			}
		};

		for (u32 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
		{
			const VizUsage* pLastWriter = nullptr;
			std::vector<const VizUsage*> readersSinceWrite;
			for (const VizUsage& usage : graph.resources[resourceIndex].usages)
			{
				const bool isRead = IsReadAccess(usage.access);
				const bool isWrite = IsWriteAccess(usage.access);

				if (isRead && pLastWriter != nullptr)
				{
					addHazard(resourceIndex, *pLastWriter, usage, HAZARD_RAW);
				}

				if (!isWrite)
				{
					readersSinceWrite.push_back(&usage);
					continue;
				}

				bool hasOtherReaders = false;
				for (const VizUsage* pReader : readersSinceWrite)
				{
					hasOtherReaders |= (pReader->passIndex != usage.passIndex);
					addHazard(resourceIndex, *pReader, usage, HAZARD_WAR);
				}

				if (!isRead && !hasOtherReaders && pLastWriter != nullptr)
				{
					addHazard(resourceIndex, *pLastWriter, usage, HAZARD_WAW);
				}

				pLastWriter = &usage;
				readersSinceWrite.clear();
			}
		}

		// Edges always point to a higher pass index, so sorting by destination guarantees every
		// source pass is fully resolved before it's used in the level calculation below
		std::sort(edges.begin(), edges.end(), [](const PassEdge& a, const PassEdge& b)
		{
			return (a.dstPass != b.dstPass) ? (a.dstPass < b.dstPass) : (a.srcPass < b.srcPass);
		});

		// ---- Dependency levels (stages) ----
		// Level = 1 + max(level of dependencies). Passes on the same level have no hazards
		// between them, so the barriers allow them to execute in parallel
		std::vector<u32> passLevels(passCount, 1);
		std::vector<u32> criticalPreds(passCount, U32_MAX);
		for (const PassEdge& edge : edges)
		{
			if (passLevels[edge.srcPass] + 1 > passLevels[edge.dstPass])
			{
				passLevels[edge.dstPass] = passLevels[edge.srcPass] + 1;
				criticalPreds[edge.dstPass] = edge.srcPass;
			}
		}

		// ---- Stats ----
		u32 levelCount = 0;
		for (u32 passIndex : graph.submissionOrder)
		{
			levelCount = Max(levelCount, passLevels[passIndex]);
		}

		std::vector<u32> levelWidths(levelCount + 1, 0);
		u32 maxWidth = 0;
		for (u32 passIndex : graph.submissionOrder)
		{
			maxWidth = Max(maxWidth, ++levelWidths[passLevels[passIndex]]);
		}

		u32 hazardCount = 0;
		u32 falseHazardCount = 0;
		for (const PassEdge& edge : edges)
		{
			if (!isExecutedPass(edge.dstPass))
			{
				continue;
			}

			for (const HazardResource& hazardResource : edge.resources)
			{
				hazardCount++;
				falseHazardCount += ((hazardResource.hazards & HAZARD_RAW) == 0) ? 1 : 0;
			}
		}

		// Critical path: longest dependency chain, ending at the deepest executed pass (preferring the final pass)
		u32 criticalEnd = U32_MAX;
		for (u32 passIndex : graph.submissionOrder)
		{
			if (criticalEnd == U32_MAX || passLevels[passIndex] > passLevels[criticalEnd] ||
				(passLevels[passIndex] == passLevels[criticalEnd] && passIndex == graph.finalPassIndex))
			{
				criticalEnd = passIndex;
			}
		}

		std::vector<u32> criticalPath;
		std::vector<bool> isOnCriticalPath(passCount, false);
		for (u32 passIndex = criticalEnd; passIndex != U32_MAX; passIndex = criticalPreds[passIndex])
		{
			criticalPath.push_back(passIndex);
			isOnCriticalPath[passIndex] = true;
		}
		std::reverse(criticalPath.begin(), criticalPath.end());

		for (PassEdge& edge : edges)
		{
			edge.isCritical = isOnCriticalPath[edge.srcPass] && isOnCriticalPath[edge.dstPass] && (criticalPreds[edge.dstPass] == edge.srcPass);
		}

		std::string criticalPathStr;
		for (u32 passIndex : criticalPath)
		{
			criticalPathStr += criticalPathStr.empty() ? "" : " &#8594; ";
			criticalPathStr += EscapeHtml(graph.passes[passIndex].name);
		}

		char parallelismStr[16];
		std::snprintf(parallelismStr, sizeof(parallelismStr), "%.2f", (levelCount > 0) ? static_cast<float>(executedCount) / static_cast<float>(levelCount) : 0.0f);

		char graphHashLine[48];
		std::snprintf(graphHashLine, sizeof(graphHashLine), "// graph_hash: %016llx\n", static_cast<unsigned long long>(graph.graphHash));

		// ===========================================================================================
		// Main graph: passes as nodes, grouped into stages (columns), connected by reduced hazard edges
		// ===========================================================================================
		std::ostringstream dot;
		dot << graphHashLine;
		dot << "digraph RenderGraph {\n";
		dot << "\trankdir=LR;\n";
		dot << "\tnewrank=true;\n";
		dot << "\tbgcolor=\"" << COLOR_BG << "\";\n";
		dot << "\tnodesep=0.35;\n";
		dot << "\tranksep=1.0;\n";
		dot << "\tfontname=\"Helvetica\";\n";
		dot << "\tnode [fontname=\"Helvetica\", fontsize=11, fontcolor=\"" << COLOR_TEXT_PRIMARY << "\"];\n";
		dot << "\tedge [fontname=\"Helvetica\", fontsize=9, arrowsize=0.8];\n\n";

		auto emitPassNode = [&](u32 passIndex, const char* indent)
		{
			const VizPass& pass = graph.passes[passIndex];
			const bool isExecuted = isExecutedPass(passIndex);
			const bool isFinalPass = (passIndex == graph.finalPassIndex);

			// Greyed passes use the grey hue directly; executed passes use a bright shade
			const char* hue = getPassHue(passIndex);
			const std::string fillColor = isExecuted ? ShiftColor(hue, SHADE_BRIGHT) : std::string(hue);

			std::string subtitle = PassTypeToString(pass.passType);
			if (isExecuted)  subtitle += " &#8226; #" + std::to_string(submissionSlots[passIndex] + 1);
			else             subtitle += " &#8226; TRIMMED";

			if (isFinalPass) subtitle += " &#8226; FINAL";

			dot << indent << "pass" << passIndex
				<< " [shape=box, style=\"filled,rounded\", fontcolor=\"" << COLOR_TEXT_PRIMARY << "\", margin=\"0.25,0.14\""
				<< ", fillcolor=\"" << fillColor << "\"";
			if (isFinalPass)  dot << ", penwidth=3, color=\"" << HUE_RED << "\"";
			else              dot << ", penwidth=1, color=\"" << COLOR_BORDER_DEFAULT << "\"";

			dot << ", label=<<b>" << EscapeHtml(pass.name) << "</b><br/><font point-size=\"9\">" << subtitle << "</font>>];\n";
		};

		// ---- Trimmed passes (outside of the stage columns) ----
		dot << "\t// Trimmed passes\n";
		for (u32 passIndex = 0; passIndex < passCount; passIndex++)
		{
			if (graph.passes[passIndex].name == nullptr || isExecutedPass(passIndex))
			{
				continue;
			}

			emitPassNode(passIndex, "\t");
		}

		// ---- Stages: one column per dependency level ----
		dot << "\n\t// Stages (passes within a stage have no hazards between them)\n";
		for (u32 level = 1; level <= levelCount; level++)
		{
			if (levelWidths[level] == 0)
			{
				continue;
			}

			dot << "\tsubgraph cluster_stage" << level << " {\n";
			dot << "\t\tlabel=\"Stage " << level << " (" << levelWidths[level] << (levelWidths[level] == 1 ? " pass" : " passes") << ")\";\n";
			dot << "\t\tstyle=\"dashed,rounded\";\n";
			dot << "\t\tcolor=\"" << COLOR_BORDER_DIM << "\";\n";
			dot << "\t\tfontcolor=\"" << COLOR_TEXT_SECONDARY << "\";\n";
			dot << "\t\tfontsize=10;\n";
			dot << "\t\trank=same;\n";
			for (u32 passIndex : graph.submissionOrder)
			{
				if (passLevels[passIndex] == level)
				{
					emitPassNode(passIndex, "\t\t");
				}
			}
			dot << "\t}\n";
		}

		// ---- Hazard edges ----
		dot << "\n\t// Dependencies (green = RAW, orange = WAR/WAW only)\n";
		for (const PassEdge& edge : edges)
		{
			const bool isGreyed = !isExecutedPass(edge.srcPass) || !isExecutedPass(edge.dstPass);
			const char* hue = (edge.hazards & HAZARD_RAW) ? HUE_GREEN : HUE_ORANGE;

			// Tooltip shows the barrier inserted before the destination pass for each resource
			std::string label;
			std::string tooltip;
			for (const HazardResource& hazardResource : edge.resources)
			{
				const std::string resourceName = EscapeDot(GetResourceDisplayName(graph.resources[hazardResource.resourceIndex], hazardResource.resourceIndex));
				const std::string hazardStr = HazardsToString(hazardResource.hazards);

				label += (label.empty() ? "" : "\\n") + resourceName + " (" + hazardStr + ")";
				tooltip += (tooltip.empty() ? "" : "\\n\\n") + resourceName + " [" + hazardStr + "]";
				for (const std::string& line : hazardResource.pDstUsage->barrierLines)
				{
					tooltip += "\\n" + EscapeDot(line);
				}
			}

			dot << "\tpass" << edge.srcPass << " -> pass" << edge.dstPass
				<< " [color=\"" << ShiftColor(hue, SHADE_BRIGHT) << "\", penwidth=" << (edge.isCritical ? "3" : "1.4")
				<< ", label=\"" << label << "\", fontcolor=\"" << ShiftColor(hue, SHADE_LIGHT) << "\""
				<< ", labeltooltip=\"" << tooltip << "\", edgetooltip=\"" << tooltip << "\"";
			if (isGreyed) dot << ", style=\"dashed\"";
			dot << "];\n";
		}

		// ---- Submission order (doesn't affect layout) ----
		dot << "\n\t// Submission order\n";
		for (u32 i = 1; i < executedCount; i++)
		{
			dot << "\tpass" << graph.submissionOrder[i - 1] << " -> pass" << graph.submissionOrder[i]
				<< " [style=\"dotted\", color=\"" << COLOR_SUBMISSION << "\", penwidth=1, arrowsize=0.5, constraint=false];\n";
		}

		// ---- Legend ----
		dot << "\n\t// Legend (floating, not connected to the graph)\n";
		dot << "\tlegend [shape=box, style=filled, fillcolor=\"" << COLOR_PANEL_FILL << "\", color=\"" << COLOR_BORDER_DEFAULT << "\", fontcolor=\"" << COLOR_TEXT_PRIMARY << "\", margin=0, label=<\n";
		dot << "\t\t<TABLE BORDER=\"0\" CELLBORDER=\"0\" CELLSPACING=\"4\" CELLPADDING=\"3\">\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><B>Legend</B></TD></TR>\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"9\" COLOR=\"" << COLOR_TEXT_SECONDARY << "\">#N = submission order &#8226; Resource details in the _resources file</FONT></TD></TR>\n";

		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"10\"><B>Passes</B></FONT></TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_BLUE,   SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Graphics pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_GREEN,  SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Compute pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_ORANGE, SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Transfer pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_PURPLE, SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Ray tracing pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_TEAL,   SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">AS build pass</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << HUE_GREY                       << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Trimmed pass (not executed)</TD></TR>\n";
		dot << "\t\t<TR><TD BORDER=\"1\" COLOR=\"" << COLOR_BORDER_DIM << "\" STYLE=\"dashed\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Stage: no hazards between its passes (could overlap)</TD></TR>\n";

		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"10\"><B>Edges</B></FONT></TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_GREEN,  SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">True dependency (read-after-write)</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << ShiftColor(HUE_ORANGE, SHADE_BRIGHT) << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">False dependency (WAR / WAW, resource reuse)</TD></TR>\n";
		dot << "\t\t<TR><TD BGCOLOR=\"" << COLOR_SUBMISSION << "\" WIDTH=\"24\"> </TD><TD ALIGN=\"LEFT\">Submission order (dotted)</TD></TR>\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"9\" COLOR=\"" << COLOR_TEXT_SECONDARY << "\">Bold = critical path &#8226; Dashed = trimmed pass &#8226; Hover edges for barriers</FONT></TD></TR>\n";
		dot << "\t\t<TR><TD COLSPAN=\"2\"><FONT POINT-SIZE=\"9\" COLOR=\"" << COLOR_TEXT_SECONDARY << "\">Parallelism is an upper bound: barriers allow it, a single queue may still serialize</FONT></TD></TR>\n";
		dot << "\t\t</TABLE>>];\n";

		// ---- Header (set last so the stage clusters don't inherit it) ----
		dot << "\n\tlabelloc=\"t\";\n";
		dot << "\tlabeljust=\"l\";\n";
		dot << "\tfontsize=12;\n";
		dot << "\tfontcolor=\"" << COLOR_TEXT_PRIMARY << "\";\n";
		dot << "\tlabel=<<b>Render graph</b> &#8226; " << executedCount << " executed passes &#8226; " << levelCount << " stages &#8226; parallelism "
			<< parallelismStr << " &#8226; max width " << maxWidth
			<< "<br/><font point-size=\"10\" color=\"" << COLOR_TEXT_SECONDARY << "\">" << hazardCount << " hazards (" << falseHazardCount
			<< " false) &#8226; critical path: " << criticalPathStr << "</font>>;\n";

		dot << "}\n";

		// ===========================================================================================
		// Resource file: usage matrix (rows = resources, columns = passes in submission order)
		// ===========================================================================================
		const std::vector<u32>& matrixColumns = graph.submissionOrder;
		const u32 columnCount = executedCount;

		std::vector<u32> passColumns(passCount, U32_MAX);
		for (u32 column = 0; column < columnCount; column++)
		{
			passColumns[matrixColumns[column]] = column;
		}

		std::ostringstream resDot;
		resDot << graphHashLine;
		resDot << "digraph RenderGraphResources {\n";
		resDot << "\tbgcolor=\"" << COLOR_BG << "\";\n";
		resDot << "\tfontname=\"Helvetica\";\n";
		resDot << "\tnode [shape=plaintext, fontname=\"Helvetica\", fontsize=10, fontcolor=\"" << COLOR_TEXT_PRIMARY << "\"];\n\n";
		resDot << "\tmatrix [label=<\n";
		resDot << "\t\t<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"5\" COLOR=\"" << COLOR_BORDER_DIM << "\">\n";

		// Header row
		resDot << "\t\t<TR><TD BGCOLOR=\"" << COLOR_PANEL_FILL << "\" ALIGN=\"LEFT\"><B>Resource</B></TD>";
		for (u32 passIndex : matrixColumns)
		{
			const VizPass& pass = graph.passes[passIndex];
			resDot << "<TD BGCOLOR=\"" << ShiftColor(getPassHue(passIndex), SHADE_BRIGHT) << "\"><B>#" << (submissionSlots[passIndex] + 1) << "</B><BR/><FONT POINT-SIZE=\"9\">"
				<< EscapeHtml(pass.name) << "</FONT><BR/><FONT POINT-SIZE=\"8\">Stage " << passLevels[passIndex] << "</FONT></TD>";
		}
		resDot << "</TR>\n";

		// One row per resource
		for (u32 resourceIndex = 0; resourceIndex < resourceCount; resourceIndex++)
		{
			const VizResource& resource = graph.resources[resourceIndex];

			// Place each usage in its column, plus track the first/last used columns to shade the resource's lifetime
			std::vector<const VizUsage*> columnUsages(columnCount, nullptr);
			u32 firstColumn = U32_MAX;
			u32 lastColumn = 0;
			for (const VizUsage& usage : resource.usages)
			{
				const u32 column = (usage.passIndex < passCount) ? passColumns[usage.passIndex] : U32_MAX;
				if (column == U32_MAX || columnUsages[column] != nullptr)
				{
					continue;
				}

				columnUsages[column] = &usage;
				firstColumn = Min(firstColumn, column);
				lastColumn = Max(lastColumn, column);
			}

			if (firstColumn == U32_MAX)
			{
				// Not used by any shown pass
				continue;
			}

			const VizResourceStyle style = GetResourceStyle(resource);
			resDot << "\t\t<TR><TD ALIGN=\"LEFT\" BGCOLOR=\"" << ShiftColor(style.hue, SHADE_DARK) << "\"><B>" << EscapeHtml(GetResourceDisplayName(resource, resourceIndex))
				<< "</B><BR/><FONT POINT-SIZE=\"8\" COLOR=\"" << COLOR_TEXT_SECONDARY << "\">" << style.tag << "</FONT></TD>";

			for (u32 column = 0; column < columnCount; column++)
			{
				const VizUsage* pUsage = columnUsages[column];
				if (pUsage == nullptr)
				{
					// Shaded if the resource is alive (between its first and last usage)
					const bool isAlive = (column > firstColumn && column < lastColumn);
					resDot << "<TD BGCOLOR=\"" << (isAlive ? COLOR_PANEL_FILL : COLOR_BG) << "\"> </TD>";
					continue;
				}

				const VizPass& pass = graph.passes[pUsage->passIndex];
				const bool isRead = IsReadAccess(pUsage->access);
				const bool isWrite = IsWriteAccess(pUsage->access);
				const char* cellHue = (isRead && isWrite) ? HUE_TEAL : (isWrite ? HUE_GREEN : HUE_BLUE);

				std::string text = std::string("<B>") + ((isRead && isWrite) ? "RW" : (isWrite ? "W" : "R")) + "</B>";
				std::vector<std::string> tooltipLines;
				tooltipLines.push_back(std::string(pass.name) + " (#" + std::to_string(submissionSlots[pUsage->passIndex] + 1) + ", stage " + std::to_string(passLevels[pUsage->passIndex]) + ")");
				tooltipLines.push_back(std::string("access: ") + AccessToString(pUsage->access));

				if (!pUsage->layout.empty())
				{
					text += "<BR/>" + EscapeHtml(pUsage->layout);
					tooltipLines.push_back("layout: " + pUsage->layout);
				}

				if (resource.type == RESOURCE_TYPE::TEXTURE && pUsage->attachmentType != ATTACHMENT_TYPE::INVALID && pass.passType == PASS_TYPE::GRAPHICS)
				{
					const std::string ops = std::string(LoadOpToString(pUsage->loadOp)) + "/" + StoreOpToString(pUsage->storeOp);
					text += std::string("<BR/><FONT POINT-SIZE=\"8\">") + AttachmentTypeToString(pUsage->attachmentType) + " " + ops + "</FONT>";
					tooltipLines.push_back(std::string("attachment: ") + AttachmentTypeToString(pUsage->attachmentType));
					tooltipLines.push_back("load/store: " + ops);
				}
				else if (resource.type == RESOURCE_TYPE::BUFFER)
				{
					char usageStr[32];
					std::snprintf(usageStr, sizeof(usageStr), "buffer usage: 0x%X", static_cast<u32>(pUsage->bufferUsage));
					tooltipLines.push_back(usageStr);
				}

				if (!pUsage->barrierLines.empty())
				{
					tooltipLines.push_back("");
					tooltipLines.push_back("barrier before this pass:");
					tooltipLines.insert(tooltipLines.end(), pUsage->barrierLines.begin(), pUsage->barrierLines.end());
				}

				std::string tooltip;
				for (const std::string& line : tooltipLines)
				{
					tooltip += (tooltip.empty() ? "" : "&#10;") + EscapeHtml(line);
				}

				resDot << "<TD BGCOLOR=\"" << ShiftColor(cellHue, SHADE_DARK) << "\" HREF=\"#\" TOOLTIP=\"" << tooltip << "\">" << text << "</TD>";
			}
			resDot << "</TR>\n";
		}

		resDot << "\t\t</TABLE>>];\n";

		resDot << "\n\tlabelloc=\"t\";\n";
		resDot << "\tlabeljust=\"l\";\n";
		resDot << "\tfontsize=12;\n";
		resDot << "\tfontcolor=\"" << COLOR_TEXT_PRIMARY << "\";\n";
		resDot << "\tlabel=<<b>Resource usage chains</b><br/><font point-size=\"9\" color=\"" << COLOR_TEXT_SECONDARY << "\">"
			<< "Columns = executed passes in submission order (trimmed passes omitted) &#8226; R = read (blue), W = write (green), RW = read-write (teal)"
			<< " &#8226; Shaded = alive between usages &#8226; Hover a cell for usage + barrier details</font>>;\n";
		resDot << "}\n";

		// ---- Write files ----
		// The resource file sits next to the main one: "graph.dot" -> "graph_resources.dot"
		std::string resourcesFileName = fileName;
		const size_t extensionPos = resourcesFileName.rfind(".dot");
		if (extensionPos != std::string::npos && extensionPos == resourcesFileName.size() - 4)
		{
			resourcesFileName.insert(extensionPos, "_resources");
		}
		else
		{
			resourcesFileName += "_resources.dot";
		}

		if (!WriteVisualizationFile(fileName, dot.str()) || !WriteVisualizationFile(resourcesFileName, resDot.str()))
		{
			return STATUS_CODE::ERR_INTERNAL;
		}

		LogDebug("Render graph visualization written to \"%s\" and \"%s\"", fileName, resourcesFileName.c_str());
		return STATUS_CODE::SUCCESS;
	}
}
