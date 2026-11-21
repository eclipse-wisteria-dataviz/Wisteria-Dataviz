#include "../../src/ui/tellmeregistry.h"
#include <catch2/catch_test_macros.hpp>
#include <wx/sstream.h>

using namespace Wisteria::UI;

// clang-format off
static const wxString FEATURE_XML{
    L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    L"<tellme>"
    L"  <category name=\"Insert Graph\">"
    L"    <item id=\"bar\" icon=\"images/bar.svg\" phrases=\"Bar chart; Insert bar chart ;;Column chart\"/>"
    L"    <item id=\"line\" icon=\"images/line.svg\" phrases=\"Line plot;Insert line chart\"/>"
    L"    <item id=\"pie\" phrases=\"Pie chart\"/>"
    L"    <item id=\"unregistered\" phrases=\"Chart without an action\"/>"
    L"    <item phrases=\"No ID\"/>"
    L"    <item id=\"empty\" phrases=\" ; \"/>"
    L"  </category>"
    L"  <category name=\"Data\">"
    L"    <item id=\"import\" phrases=\"Import data;Open a dataset\"/>"
    L"    <item id=\"bar\" phrases=\"Duplicate ID\"/>"
    L"  </category>"
    L"  <category name=\"Help\">"
    L"    <item id=\"help-bar\" help=\"online/bar.html\" phrases=\"Bar chart help\"/>"
    L"  </category>"
    L"</tellme>"
};

// clang-format on

static void LoadTestRegistry(TellMeRegistry& registry)
    {
    wxStringInputStream stream{ FEATURE_XML };
    REQUIRE(registry.LoadXml(stream));
    }

TEST_CASE("Tell Me registry loads XML", "[tellmeregistry]")
    {
    TellMeRegistry registry;
    LoadTestRegistry(registry);

    const auto& categories = registry.GetCategories();
    REQUIRE(categories.size() == 3);
    CHECK(categories[0].m_name == L"Insert Graph");
    CHECK(categories[1].m_name == L"Data");
    CHECK(categories[2].m_name == L"Help");

    // features with no ID or no phrases are skipped
    REQUIRE(categories[0].m_items.size() == 4);
    // duplicate IDs are skipped
    REQUIRE(categories[1].m_items.size() == 1);

    const auto barPos = registry.FindItem(L"bar");
    REQUIRE(barPos.has_value());
    CHECK(barPos->first == 0);
    CHECK(barPos->second == 0);
    const auto& bar = registry.GetItem(barPos->first, barPos->second);
    CHECK(bar.m_iconPath == L"images/bar.svg");
    // phrases are trimmed and empty ones dropped
    REQUIRE(bar.m_phrases.size() == 3);
    CHECK(bar.m_phrases[0] == L"Bar chart");
    CHECK(bar.m_phrases[1] == L"Insert bar chart");
    CHECK(bar.m_phrases[2] == L"Column chart");

    CHECK(registry.GetItem(2, 0).m_helpTopic == L"online/bar.html");
    CHECK_FALSE(registry.FindItem(L"empty").has_value());
    CHECK_FALSE(registry.FindItem(L"missing").has_value());
    }

TEST_CASE("Tell Me registry rejects bad XML", "[tellmeregistry]")
    {
    TellMeRegistry registry;
    wxStringInputStream wrongRoot{ L"<settings><category name=\"A\"/></settings>" };
    CHECK_FALSE(registry.LoadXml(wrongRoot));
    wxStringInputStream malformed{ L"<tellme><category" };
    CHECK_FALSE(registry.LoadXml(malformed));
    CHECK(registry.GetCategories().empty());
    }

TEST_CASE("Tell Me registry merges categories across files", "[tellmeregistry]")
    {
    TellMeRegistry registry;
    LoadTestRegistry(registry);
    wxStringInputStream extra{
        L"<tellme><category name=\"Data\"><item id=\"export\" phrases=\"Export data\"/>"
        L"</category></tellme>"
    };
    REQUIRE(registry.LoadXml(extra));
    REQUIRE(registry.GetCategories().size() == 3);
    CHECK(registry.GetCategories()[1].m_items.size() == 2);
    const auto exportPos = registry.FindItem(L"export");
    REQUIRE(exportPos.has_value());
    CHECK(exportPos->first == 1);
    CHECK(exportPos->second == 1);
    }

TEST_CASE("Tell Me registry search", "[tellmeregistry]")
    {
    TellMeRegistry registry;
    LoadTestRegistry(registry);
    const auto noOp = [](wxWindow*) {};
    registry.RegisterAction(L"bar", noOp);
    registry.RegisterAction(L"line", noOp);
    registry.RegisterAction(L"pie", noOp);
    registry.RegisterAction(L"import", noOp);

    SECTION("Empty query")
        {
        const auto results = registry.Search(L"   ");
        CHECK_FALSE(results.m_bestMatch.has_value());
        CHECK(results.m_groups.empty());
        }

    SECTION("No match")
        {
        const auto results = registry.Search(L"zzz");
        CHECK_FALSE(results.m_bestMatch.has_value());
        CHECK(results.m_groups.empty());
        }

    SECTION("Exact match ranks first and is case insensitive")
        {
        const auto results = registry.Search(L"PIE CHART");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        CHECK(registry.GetItem(best.m_categoryIndex, best.m_itemIndex).m_id == L"pie");
        CHECK(best.m_score == 100);
        }

    SECTION("One result per feature, using its best phrase")
        {
        // "bar chart" and "insert bar chart" both match; the prefix match wins
        const auto results = registry.Search(L"bar");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        CHECK(registry.GetItem(best.m_categoryIndex, best.m_itemIndex).m_id == L"bar");
        CHECK(best.m_phraseIndex == 0);
        CHECK(best.m_score == 80);
        for (const auto& [catIndex, group] : results.m_groups)
            {
            for (const auto& match : group)
                {
                CHECK(registry.GetItem(match.m_categoryIndex, match.m_itemIndex).m_id != L"bar");
                }
            }
        }

    SECTION("Multiple tokens must all match")
        {
        const auto results = registry.Search(L"insert line");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        CHECK(registry.GetItem(best.m_categoryIndex, best.m_itemIndex).m_id == L"line");
        CHECK(best.m_score == 80);
        CHECK(results.m_groups.empty());

        // tokens in any order match at word starts
        const auto reversed = registry.Search(L"chart insert");
        REQUIRE(reversed.m_bestMatch.has_value());
        CHECK(reversed.m_bestMatch->m_score == 60);
        }

    SECTION("Mid-word matches score lowest")
        {
        const auto results = registry.Search(L"olumn");
        REQUIRE(results.m_bestMatch.has_value());
        CHECK(results.m_bestMatch->m_score == 40);
        }

    SECTION("Highlights are sorted and merged")
        {
        const auto results = registry.Search(L"chart bar ba");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        // "Bar chart": "bar" and "ba" overlap at 0, "chart" at 4
        REQUIRE(best.m_phraseIndex == 0);
        REQUIRE(best.m_highlights.size() == 2);
        CHECK(best.m_highlights[0] == std::pair<size_t, size_t>{ 0, 3 });
        CHECK(best.m_highlights[1] == std::pair<size_t, size_t>{ 4, 5 });
        }

    SECTION("Highlights are in the display name when another phrase matched")
        {
        const auto results = registry.Search(L"insert bar");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        CHECK(registry.GetItem(best.m_categoryIndex, best.m_itemIndex).m_id == L"bar");
        // "Insert bar chart" matched, but only "bar" is in "Bar chart"
        CHECK(best.m_phraseIndex == 1);
        REQUIRE(best.m_highlights.size() == 1);
        CHECK(best.m_highlights[0] == std::pair<size_t, size_t>{ 0, 3 });

        // a synonym that shares no words with the display name highlights nothing
        const auto synonym = registry.Search(L"column");
        REQUIRE(synonym.m_bestMatch.has_value());
        CHECK(synonym.m_bestMatch->m_phraseIndex == 2);
        CHECK(synonym.m_bestMatch->m_highlights.empty());
        }

    SECTION("Groups follow category order and respect limits")
        {
        const auto results = registry.Search(L"a");
        REQUIRE(results.m_bestMatch.has_value());
        size_t lastCategory{ 0 };
        for (const auto& [catIndex, group] : results.m_groups)
            {
            CHECK(catIndex >= lastCategory);
            lastCategory = catIndex;
            CHECK(group.size() <= 5);
            }

        const auto limited = registry.Search(L"a", 1, 2);
        size_t total{ limited.m_bestMatch.has_value() ? 1U : 0U };
        for (const auto& [catIndex, group] : limited.m_groups)
            {
            CHECK(group.size() <= 1);
            total += group.size();
            }
        CHECK(total <= 2);
        }

    SECTION("Features without an action are not searched")
        {
        const auto results = registry.Search(L"without an action");
        CHECK_FALSE(results.m_bestMatch.has_value());
        }

    SECTION("Help features need a help handler")
        {
        CHECK_FALSE(registry.Search(L"help").m_bestMatch.has_value());
        registry.SetHelpHandler([](wxWindow*, const wxString&) {});
        const auto results = registry.Search(L"help");
        REQUIRE(results.m_bestMatch.has_value());
        const auto& best = results.m_bestMatch.value();
        CHECK(registry.GetItem(best.m_categoryIndex, best.m_itemIndex).m_id == L"help-bar");
        }
    }

TEST_CASE("Tell Me registry runs actions", "[tellmeregistry]")
    {
    TellMeRegistry registry;
    LoadTestRegistry(registry);

    wxString lastRun;
    bool importEnabled{ false };
    registry.RegisterAction(L"bar", [&lastRun](wxWindow*) { lastRun = L"bar"; });
    registry.RegisterAction(L"line", [&lastRun](wxWindow*) { lastRun = L"line"; });
    registry.RegisterAction(
        L"import", [&lastRun](wxWindow*) { lastRun = L"import"; },
        [&importEnabled](wxWindow*) { return importEnabled; });

    SECTION("Run calls the registered lambda")
        {
        CHECK(registry.Run(L"line", nullptr));
        CHECK(lastRun == L"line");
        CHECK(registry.Run(L"bar", nullptr));
        CHECK(lastRun == L"bar");
        }

    SECTION("Unregistered features do not run")
        {
        CHECK_FALSE(registry.IsRunnable(L"unregistered"));
        CHECK_FALSE(registry.Run(L"unregistered", nullptr));
        CHECK(registry.GetRecentIds().empty());
        }

    SECTION("Disabled features do not run")
        {
        CHECK_FALSE(registry.IsEnabled(L"import", nullptr));
        CHECK_FALSE(registry.Run(L"import", nullptr));
        CHECK(lastRun.empty());
        importEnabled = true;
        CHECK(registry.Run(L"import", nullptr));
        CHECK(lastRun == L"import");
        }

    SECTION("Help features route to the help handler")
        {
        wxString openedTopic;
        registry.SetHelpHandler([&openedTopic](wxWindow*, const wxString& topic)
                                { openedTopic = topic; });
        CHECK(registry.Run(L"help-bar", nullptr));
        CHECK(openedTopic == L"online/bar.html");
        }

    SECTION("Recent list is newest first and deduplicated")
        {
        registry.Run(L"bar", nullptr);
        registry.Run(L"line", nullptr);
        registry.Run(L"bar", nullptr);
        const auto& recent = registry.GetRecentIds();
        REQUIRE(recent.size() == 2);
        CHECK(recent[0] == L"bar");
        CHECK(recent[1] == L"line");
        }

    SECTION("Recent list is capped")
        {
        std::vector<wxString> ids;
        for (size_t i = 0; i < TellMeRegistry::MAX_RECENT_IDS + 5; ++i)
            {
            ids.push_back(wxString::Format(L"id%zu", i));
            }
        ids.insert(std::next(ids.begin()), L"id0");
        registry.SetRecentIds(ids);
        const auto& recent = registry.GetRecentIds();
        REQUIRE(recent.size() == TellMeRegistry::MAX_RECENT_IDS);
        CHECK(recent[0] == L"id0");
        CHECK(recent[1] == L"id1");

        registry.Run(L"bar", nullptr);
        CHECK(registry.GetRecentIds().size() == TellMeRegistry::MAX_RECENT_IDS);
        CHECK(registry.GetRecentIds().front() == L"bar");
        }
    }
