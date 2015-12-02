#include <catch.hpp>

#include "helper/io.hpp"

#include <database.hpp>
#include <errors.hpp>
#include <package.hpp>

#include <string>

using namespace std;

static const char *M = "[package]";

TEST_CASE("empty package name", M) {
  try {
    Package pack(Package::ScriptType, string());
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(string(e.what()) == "empty package name");
  }
}

TEST_CASE("package versions are sorted", M) {
  Package pack(Package::ScriptType, "a");
  CHECK(pack.versions().size() == 0);

  Source *sourceA = new Source(Source::GenericPlatform, "google.com");
  Source *sourceB = new Source(Source::GenericPlatform, "google.com");

  Version *final = new Version("1");
  final->addSource(sourceA);

  Version *alpha = new Version("0.1");
  alpha->addSource(sourceB);

  pack.addVersion(final);
  CHECK(pack.versions().size() == 1);

  pack.addVersion(alpha);
  CHECK(pack.versions().size() == 2);

  REQUIRE(pack.version(0) == alpha);
  REQUIRE(pack.version(1) == final);
  REQUIRE(pack.lastVersion() == final);
}

TEST_CASE("drop empty version", M) {
  Package pack(Package::ScriptType, "a");
  pack.addVersion(new Version("1"));

  REQUIRE(pack.versions().empty());
}

TEST_CASE("unknown target location", M) {
  Package pack(Package::UnknownType, "a");

  try {
    pack.targetLocation();
    FAIL();
  }
  catch(const reapack_error &e) {
    REQUIRE(string(e.what()) == "unsupported package type");
  }
}

TEST_CASE("script target location", M) {
  Category cat("Category Name");

  Package pack(Package::ScriptType, "file.name");
  pack.setCategory(&cat);

  const Path path = pack.targetLocation();

  Path expected;
  expected.append("Scripts");
  expected.append("ReaScripts");
  expected.append("Category Name");
  expected.append("file.name");

  REQUIRE(path == expected);
}

TEST_CASE("script target location without category", M) {
  Package pack(Package::ScriptType, "file.name");

  const Path path = pack.targetLocation();

  Path expected;
  expected.append("Scripts");
  expected.append("ReaScripts");
  expected.append("file.name");

  REQUIRE(path == expected);
}