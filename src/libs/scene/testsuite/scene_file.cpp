#include "storm/scene/scene.hpp"

#include <catch2/catch_all.hpp>

using namespace storm::scene;

TEST_CASE("A scene is written and read back the same", "[scene]")
{
    SceneObject crate;
    crate.name = "crate";
    crate.geometry = "demo\\crate";
    crate.technique = "DLightModel";
    // a turn about y by 90 degrees, 1.5 m along x
    const float matrix[16] = {0, 0, -1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1.5f, 0.25f, -3, 1};
    std::copy(matrix, matrix + 16, crate.matrix);
    SceneObject deck;
    deck.name = "deck";
    deck.geometry = "demo\\deck";

    const std::string text = FormatScene({crate, deck});
    std::string error;
    const auto objects = ParseScene(text, error);
    REQUIRE(objects);
    REQUIRE(objects->size() == 2);
    CHECK((*objects)[0].SameAs(crate));
    CHECK((*objects)[1].SameAs(deck));
    CHECK((*objects)[1].technique.empty());
    CHECK(FormatScene(*objects) == text);
}

TEST_CASE("A scene by hand: the place may be left out", "[scene]")
{
    std::string error;
    const auto objects = ParseScene(R"([[model]]
name = "barrel"
geometry = "demo\\barrel"
position = [1, 0, 2.5]
)",
                                    error);
    REQUIRE(objects);
    REQUIRE(objects->size() == 1);
    CHECK((*objects)[0].matrix[12] == 1.f);
    CHECK((*objects)[0].matrix[14] == 2.5f);
    CHECK((*objects)[0].matrix[0] == 1.f);
}

TEST_CASE("A broken scene says why", "[scene]")
{
    std::string error;
    CHECK(!ParseScene("[[model]]\nname = \"x\"\n", error));
    CHECK(error.find("geometry") != std::string::npos);
    CHECK(!ParseScene("[[model]\n", error));
    CHECK(!error.empty());
    CHECK(!ParseScene("[[model]]\ngeometry = \"a\"\nposition = [1, 2]\n", error));
}

TEST_CASE("An empty scene", "[scene]")
{
    std::string error;
    const auto objects = ParseScene(FormatScene({}), error);
    REQUIRE(objects);
    CHECK(objects->empty());
}

TEST_CASE("Animated models and markers are written and read back", "[scene]")
{
    SceneObject sailor;
    sailor.name = "watch";
    sailor.geometry = "characters\\sailor";
    sailor.textures = "characters";
    sailor.animation = "fencer";
    sailor.action = "idle_1";
    SceneObject start;
    start.name = "player";
    start.marker = true;
    // turned to face -z
    const float matrix[16] = {-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 2, 0, -5, 1};
    std::copy(matrix, matrix + 16, start.matrix);

    // the markers go after the models
    const std::string text = FormatScene({start, sailor});
    CHECK(text.find("[[marker]]") > text.find("[[model]]"));
    CHECK(text.find("animation = 'fencer'") != std::string::npos);
    CHECK(text.find("textures = 'characters'") != std::string::npos);
    std::string error;
    const auto objects = ParseScene(text, error);
    REQUIRE(objects);
    REQUIRE(objects->size() == 2);
    CHECK((*objects)[0].SameAs(sailor));
    CHECK((*objects)[1].SameAs(start));
    CHECK((*objects)[1].geometry.empty());
    CHECK(!ParseScene("[[marker]]\nposition = [0.0, 0.0, 0.0]\n", error));
    CHECK(error.find("name") != std::string::npos);
}
