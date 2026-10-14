#include "dialog/dialog_utils.hpp"

#include <catch2/catch_all.hpp>

using namespace storm::dialog;

TEST_CASE("SplitIntoPages", "[dialog]")
{
    SECTION("No breaks")
    {
        const auto page_breaks = SplitIntoPages(5, 5);

        CHECK(page_breaks.empty());
    }

    SECTION("No forced breaks")
    {
        const auto page_breaks = SplitIntoPages(10, 3);

        REQUIRE(page_breaks.size() == 3);
        CHECK(page_breaks[0] == 3);
        CHECK(page_breaks[1] == 6);
        CHECK(page_breaks[2] == 9);
    }

    SECTION("Forced breaks")
    {
        const auto page_breaks = SplitIntoPages(10, 3, {2, 6});

        REQUIRE(page_breaks.size() == 4);
        CHECK(page_breaks[0] == 2);
        CHECK(page_breaks[1] == 5);
        CHECK(page_breaks[2] == 6);
        CHECK(page_breaks[3] == 9);
    }
}

TEST_CASE("AddToStringArrayLimitedByWidth", "[dialog]")
{
    // one unit per character
    const auto width = [](const std::string_view &text) { return static_cast<int32_t>(text.size()); };

    SECTION("Keeps a line that fits")
    {
        std::vector<std::string> lines;
        AddToStringArrayLimitedByWidth("aaaaa bbbbb ccccc dd", 20, lines, width);

        REQUIRE(lines.size() == 1);
        CHECK(lines[0] == "aaaaa bbbbb ccccc dd");
    }

    SECTION("Wraps the last word")
    {
        std::vector<std::string> lines;
        AddToStringArrayLimitedByWidth("aaaaa bbbbb ccccc dddddddd", 20, lines, width);

        REQUIRE(lines.size() == 2);
        CHECK(lines[0] == "aaaaa bbbbb ccccc");
        CHECK(lines[1] == "dddddddd");
    }

    SECTION("Wraps a long text")
    {
        std::vector<std::string> lines;
        AddToStringArrayLimitedByWidth("aaaaa bbbbb ccccc ddddd eeeee fffff ggggg", 20, lines, width);

        REQUIRE(lines.size() == 3);
        CHECK(lines[0] == "aaaaa bbbbb ccccc");
        CHECK(lines[1] == "ddddd eeeee fffff");
        CHECK(lines[2] == "ggggg");
    }
}
