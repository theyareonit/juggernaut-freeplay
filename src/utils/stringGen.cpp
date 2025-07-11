#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/binding/LocalLevelManager.hpp>
#include <Geode/cocos/platform/CCFileUtils.h>
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/file.hpp>
#include <random>
#include <cmath>
#include <fmt/core.h>
#include <fmt/args.h>
#include <Geode/cocos/support/zip_support/ZipUtils.h>
#include "constants.hpp"
#include "StringGen.hpp"
#include "Theming.hpp"
#include "Ninja.hpp"

using namespace geode::prelude;

bool jfpActive = false; // used in GameManager.cpp to check if JFP is active

namespace JFPGen {

static void orientationShift(int prevO[11], int newO) {
	for (int i = 0; i < 10; i++) {
		prevO[i] = prevO[i+1];
	}
	prevO[10] = newO;
}

void exportLvlStringGMD(std::filesystem::path const& path, std::string ld1) {
    auto lvlData = ByteVector(ld1.begin(), ld1.end());
    (void) file::writeBinary(path, lvlData);
}

static int convertSpeed(SpeedChange speed) {
    switch (speed) {
        case SpeedChange::Speed05x: return 200;
        case SpeedChange::Speed1x:  return 201;
        case SpeedChange::Speed2x:  return 202;
        case SpeedChange::Speed4x:  return 1334;
        default: return 203;
    }
}

std::string jfpPackString(const std::string& level, unsigned int seed, int song, bool compress) {

    std::string b64;
    if (compress) b64 = ZipUtils::compressString(level, true, 0);
    else b64 = level;
    std::string desc = fmt::format("ninja v1");
    desc = ZipUtils::base64URLEncode(ZipUtils::base64URLEncode(desc)); // double encoding might be unnecessary according to gmd-api source?
    b64.erase(std::find(b64.begin(), b64.end(), '\0'), b64.end());
    desc.erase(std::find(desc.begin(), desc.end(), '\0'), desc.end());

    std::string levelString = fmt::format("<k>kCEK</k><i>4</i><k>k2</k><s>JFP {title}</s><k>k3</k><s>{desc}</s><k>k4</k><s>{b64}</s><k>k45</k><i>{song}</i><k>k13</k><t/><k>k21</k><i>2</i><k>k50</k><i>35</i>",
    fmt::arg("desc", desc),
    fmt::arg("b64", b64),
    fmt::arg("title", std::to_string(seed)),
    fmt::arg("song", song));

    return levelString;
}

std::string jfpStringGen(bool compress) {
    LevelData ldata = generateJFPLevel();
    if (ldata.biomes.empty()) return "";

    std::string themeString = ThemeGen::parseTheme(Mod::get()->getSettingValue<std::string>("active-theme"), ldata);
    std::string levelString = jfpNewStringGen(ldata);;
    levelString = colorStringGen() + kStringGen() + levelString + themeString;

    return jfpPackString(levelString, ldata.seed, ldata.biomes[0].song, compress);
}

std::string kStringGen() {
    std::string result = ",";
    for (const auto& [key, value] : kBank) {
        result += key + "," + value + ",";
    }
    result += ";";
    return result;
}

std::string colorStringGen() {
    std::string result = "kS38,";

    for (const auto& color : colorBank) {
        result += fmt::format(
            "1_{r}_2_{g}_3_{b}_11_255_12_255_13_255_4_-1_6_{slot}_15_1_18_0_8_1",
            fmt::arg("r", color.rgb[0]),
            fmt::arg("g", color.rgb[1]),
            fmt::arg("b", color.rgb[2]),
            fmt::arg("slot", color.slot)
        );
        if (color.blending) result += "_5_1";
        if (color.opacity <= 1.0) {
            result += fmt::format("_7_{}", color.opacity);
        } else {
            result += "_7_1";
        }
        if (color.copyColor >= 0) {
            result += fmt::format("_9_{}", color.copyColor);
        }
        if (!color.special.empty()) {
            result += "_" + color.special;
        }
        result += "|";
    }

    return result;
}

std::string jfpNewStringGen(LevelData ldata) {

    if (ldata.biomes.empty()) return "";

    std::string levelBuildSeg2 = fmt::format(
        "1,{speedID},2,255,3,165,13,1,64,1,67,1;",
        fmt::arg("speedID", convertSpeed(ldata.biomes[0].options.startingSpeed))
    );
    const bool cornerPieces = Mod::get()->getSettingValue<bool>("corners");
    
    std::string level;
    if(!overrideBank["override-base"]) level += levelBaseSeg;
    level += levelBuildSeg2;

    const auto& biome = ldata.biomes[0];
    const auto& opts = biome.options;
    int x = biome.x_initial;
    int y = biome.y_initial;
    int y_swing = 0;
    int corridorHeight = opts.corridorHeight;
    bool spikeSide = false;

    if(!overrideBank["override-base"]) {
        fmt::dynamic_format_arg_store<fmt::format_context> args;
        args.push_back(fmt::arg("ch_1", 225 + corridorHeight));
        args.push_back(fmt::arg("ch_2", 165 + corridorHeight));
        args.push_back(fmt::arg("ch_3", 195 + corridorHeight));
        args.push_back(fmt::arg("ch_4", 255 + corridorHeight));
        args.push_back(fmt::arg("ch_5", 285 + corridorHeight));
        std::string startingConnectors = fmt::vformat(
            levelStartingBase,
            args
        );
        level += startingConnectors;
    }

    for (int64_t i = 0; i < biome.segments.size(); i++) {
        const auto& seg = biome.segments[i];
        x = seg.coords.first;
        y = seg.coords.second;
        y_swing = seg.y_swing;

        // Slopes
        if (biome.options.startingSize == WaveSize::Mini) {
            level += fmt::format("1,1339,2,{x},3,{y},6,90,5,{flip},64,1,67,1;",
                fmt::arg("x", x),
                fmt::arg("y", y - 15),
                fmt::arg("flip", y_swing > 0 ? 1 : 0)
            );

            level += fmt::format("1,1339,2,{x},3,{yC},6,-90,5,{flip},64,1,67,1;",
                fmt::arg("x", x),
                fmt::arg("yC", y + 15 + corridorHeight),
                fmt::arg("flip", y_swing > 0 ? 1 : 0)
            );
        } else {
            level += fmt::format("1,1338,2,{x},3,{y},6,{rot},64,1,67,1;",
                fmt::arg("x", x),
                fmt::arg("y", y),
                fmt::arg("rot", y_swing > 0 ? 0 : 90)
            );
            
            level += fmt::format("1,1338,2,{x},3,{yC},6,{rot},64,1,67,1;",
                fmt::arg("x", x),
                fmt::arg("yC", y + corridorHeight),
                fmt::arg("rot", y_swing > 0 ? 180 : 270)
            );
        }

        // Corner-Pieces
        std::string cornerBuild = "";
        if (i > 1 && cornerPieces) {
            if (biome.segments[i - 1].y_swing == 1 && y_swing == -1) {
                cornerBuild = fmt::format("1,473,2,{cnr1x},3,{cnry},6,-180;1,473,2,{cnr2x},3,{cnry},6,-90,64,1,67,1;",
                fmt::arg("cnr1x", x - 30),
                fmt::arg("cnr2x", x),
                fmt::arg("cnry", y + corridorHeight + 30));
            } else if (
                (biome.segments[i - 1].y_swing == -1 && y_swing == 1) ||
                (i == 0 && y_swing == 1)
            ) {
                cornerBuild = fmt::format("1,473,2,{cnr1x},3,{cnry},6,90;1,473,2,{cnr2x},3,{cnry},64,1,67,1;",
                fmt::arg("cnr1x", x - 30),
                fmt::arg("cnr2x", x),
                fmt::arg("cnry", y - 30));
            }
            level += cornerBuild;
        }

        // Gravity-Portals
        if (seg.options.isPortal == Portals::Gravity) {
            double portalFactor = ((double)corridorHeight / 60.0) * 1.414;
            int portalNormal = corridorHeight / 10;
            int portalPos = corridorHeight / 4;
            int portalID = seg.options.gravity ? 11 : 10;

            int portalOrientation = y_swing;
            int mpf = 0;
            if (y_swing == biome.segments[i - 1].y_swing) {
                portalOrientation *= -1;
                mpf += 30 * portalOrientation;
            }

            std::string portalBuild = fmt::format("1,{portalID},2,{xP},3,{yP},6,{rPdeg},32,{scale},64,1,67,1;",
            fmt::arg("portalID", portalID),
            fmt::arg("xP", x-15-portalNormal+portalPos),
            fmt::arg("yP", y+mpf+(portalOrientation == 1 ? portalNormal+portalPos-15 : corridorHeight+15-portalNormal-portalPos)),
            fmt::arg("rPdeg", (portalOrientation == 1 ? 45 : -45)),
            fmt::arg("scale", portalFactor / 2.5));
            level += portalBuild;
        } else if (seg.options.isPortal == Portals::Fake) {
            double portalFactor = ((double)corridorHeight / 60.0) * 1.414;
            int portalNormal = corridorHeight / 10;
            int portalPos = corridorHeight / 4;
            int portalID = seg.options.gravity ? 10 : 11;

            std::string portalBuild = fmt::format("1,{portalID},2,{xP},3,{yP},6,{rPdeg},32,{scale},64,1,67,1;",
            fmt::arg("portalID", portalID),
            fmt::arg("xP", x-15-portalNormal+portalPos),
            fmt::arg("yP", y+(y_swing == 1 ? portalNormal+portalPos-15 : corridorHeight+15-portalNormal-portalPos)),
            fmt::arg("rPdeg", (y_swing == 1 ? 45 : -45)),
            fmt::arg("scale", portalFactor / 2.5));
            level += portalBuild;
        }

        // Speed-Changes
        if (seg.options.speedChange != SpeedChange::None) {
            int speedID = convertSpeed(seg.options.speedChange);

            int spY = y + corridorHeight/2 + 15 * (seg.y_swing == 1 ? -1 : 1);
            int spR = seg.y_swing == 1 ? -45 : 45;
            double speedFactor = 0.5 * (corridorHeight / 60.0);
            level += fmt::format("1,{speedID},2,{x},3,{y},6,{r},32,{factor},64,1,67,1,155,1;",
                    fmt::arg("speedID", speedID),
                    fmt::arg("x", x - 15),
                    fmt::arg("y", spY),
                    fmt::arg("r", spR),
                    fmt::arg("factor", speedFactor)
            );
        }

        // Corridor-Spikes
        if (seg.options.isSpikeM) {
            spikeSide = seg.options.spikeSide;
            int xS = x - 6;
            if (!spikeSide) {
                xS = x + 6;
            }
            int yS = y + 6;
            if ((y_swing == 1 && !spikeSide) || (y_swing == -1 && spikeSide)) {
                yS = y + corridorHeight - 6;
            }
            int rS = 45;
            if (y_swing == 1 && !spikeSide) {
                rS = 135;
            } else if (y_swing == 1 && spikeSide) {
                rS = -45;
            } else if (y_swing == -1 && spikeSide) {
                rS = -135;
            }
            std::string spikeBuild = fmt::format("1,103,2,{xS},3,{yS},6,{rS},64,1,67,1;",
                fmt::arg("xS", xS),
                fmt::arg("yS", yS),
                fmt::arg("rS", rS)
            );
            level += spikeBuild;
        }

        if (seg.options.isFuzzy) {
            std::string colorMod = (biome.options.colorMode == ColorMode::NightMode) ? "21,1004,41,1,43,0a1a0.60a0a0," : "";
            level += fmt::format("1,1717,2,{x},3,{y},{colorMod}6,{r1},64,1,67,1,155,7;1,1717,2,{x},3,{yC},{colorMod}6,{r2},64,1,67,1,155,7;",
                fmt::arg("x", x),
                fmt::arg("y", y),
                fmt::arg("yC", y + corridorHeight),
                fmt::arg("r1", (y_swing == 1 ? 0 : 90)),
                fmt::arg("r2", (y_swing == 1 ? 180 : 270)),
                fmt::arg("colorMod", colorMod)
            );
        }
    }
    
    // Ending-Connectors
    if (
        biome.segments.back().y_swing == 1 && !overrideBank["override-endup"] ||
        biome.segments.back().y_swing == -1 && !overrideBank["override-enddown"]
    ) {
        auto lastSegment = biome.segments[biome.segments.size() - 1];
        int xB = lastSegment.coords.first, yB = lastSegment.coords.second, xT = xB, yT = yB + corridorHeight;
        if (!biome.segments.empty() && lastSegment.y_swing == 1) {
            yB += 30;
        } else {
            yT -= 30;
        }
        while (yT <= (opts.maxHeight + corridorHeight + 30)) {
            xT += 30;
            yT += 30;
            level += fmt::format("1,1338,2,{x},3,{y},6,180,64,1,67,1;", fmt::arg("x", xT), fmt::arg("y", yT));
        }
        while (yB >= (opts.minHeight)) {
            xB += 30;
            yB -= 30;
            level += fmt::format("1,1338,2,{x},3,{y},6,90,64,1,67,1;", fmt::arg("x", xB), fmt::arg("y", yB));
        }
    }

    // Meter-Marks
    const bool marks = Mod::get()->getSettingValue<bool>("marks");
    const int64_t markInterval = Mod::get()->getSettingValue<int64_t>("marker-interval");
    std::string metermarksStr = "";
    std::string currentMark;
    if (marks && markInterval > 0) {
        int meters = markInterval;
        double markHeight;
        for (int j = 0; j < (biome.options.length / markInterval); j++) {
            currentMark = "";
            markHeight = 15.5;

            for (int i = 0; i < 10; i++) {
                currentMark += fmt::format("1,508,2,{dist},3,{markHeight},20,1,57,2,6,-90,64,1,67,1,21,1011,155,12,25,1,24,11;", fmt::arg("dist", 345+meters*30), fmt::arg("markHeight", markHeight));
                markHeight += 30.0;
            }

            std::string meterLabel = ZipUtils::base64URLEncode(fmt::format("{}m", meters));
            meterLabel.erase(std::find(meterLabel.begin(), meterLabel.end(), '\0'), meterLabel.end());
            currentMark += fmt::format("1,914,2,{dist},3,21,20,1,57,2,32,0.62,21,1011,31,{meterLabel},155,12,25,1,64,1,67,1,24,11;", fmt::arg("dist", 375+meters*30), fmt::arg("meterLabel", meterLabel));
            meters += markInterval;
            metermarksStr += currentMark;
        }
    }
    level += metermarksStr;

    // Visibility
    if (biome.options.visibility == Visibility::Low) {
        level += lowVis;
    }

    // Upside-Start
    if (biome.options.startingGravity) {
        level += "1,11,2,299,3,99,6,45,32,0.57;";
    }
    
    // log::info("LevelData: name={}", ldata.name);
    // log::info("Biomes: {}", ldata.biomes.size());
    // for (size_t i = 0; i < ldata.biomes.size(); ++i) {
    //     const auto& biome = ldata.biomes[i];
    //     log::info("  Biome {}: x_initial={}, type={}, theme={}, segments={}", i, biome.x_initial, biome.type, biome.theme, biome.segments.size());
    //     log::info("    Options: length={}, corridorHeight={}, maxHeight={}, visibility={}, startingSpeed={}, colorMode={}",
    //         biome.options.length,
    //         biome.options.corridorHeight,
    //         biome.options.maxHeight,
    //         static_cast<int>(biome.options.visibility),
    //         static_cast<int>(biome.options.startingSpeed),
    //         static_cast<int>(biome.options.colorMode)
    //     );
    //     for (size_t j = 0; j < biome.segments.size(); ++j) {
    //         const auto& seg = biome.segments[j];
    //         // log::info("      Segment {}: coords=({}, {}), y_swing={}", j, seg.coords.first, seg.coords.second, seg.y_swing);
    //     }
    // }

    return level;
}

}