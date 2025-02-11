# Buildbot Testing Configuration Files

The files in this directory control how tests are run on the
[Chromium buildbots](https://www.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot).
In addition to specifying what tests run on which builders, they also specify
special arguments and constraints for the tests.

Adding a new test suite?

The bar for adding new test suites is high. New test suites result in extra
linking time for builders, and sending binaries around to the swarming bots.
This is especially onerous for suites such as browser_tests (more than 300MB
as of this writing). Unless there is a compelling reason to have a standalone
suite, include your tests in existing test suites. For example, all
InProcessBrowserTests should be in browser_tests. Similarly any unit-tests in
components should be in components_unittests.

## A tour of the directory

* <master_name\>.json -- buildbot configuration json files. These are used to
configure what tests are run on what builders, in addition to specifying
builder-specific arguments and parameters. They are now autogenerated, mainly
using the generate_buildbot_json tool in this directory.
* [generate_buildbot_json.py](./generate_buildbot_json.py) -- generates most of
the buildbot json files in this directory, based on data contained in the
waterfalls.pyl, test_suites.pyl, and test_suite_exceptions.pyl files.
* [waterfalls.pyl](./waterfalls.pyl) -- describes the bots on the various
waterfalls, and which test suites they run. By design, this file can only refer
(by name) to test suites that are defined in test_suites.pyl.
* [test_suites.pyl](./test_suites.pyl) -- describes the test suites that are
referred to by waterfalls.pyl. A test suite describes groups of tests that are
run on one or more bots.
* [test_suite_exceptions.pyl](./test_suite_exceptions.pyl) -- describes
exceptions to the test suites, for example excluding a particular test from
running on one bot. The goal is to have very few or no exceptions, which is why
this information is factored into a separate file.
* [gn_isolate_map.pyl](./gn_isolate_map.pyl) -- maps Ninja build target names
to GN labels. Allows for certain overrides to get certain tests targets to work
with GN (and properly run when isolated).
* [trybot_analyze_config.json](./trybot_analyze_config.json) -- used to provide
exclusions to
[the analyze step](https://www.chromium.org/developers/testing/commit-queue/chromium_trybot-json)
on trybots.
* [filters/](./filters/) -- filters out tests that shouldn't be
run in a particular mode.
* [timeouts.py](./timeouts.py) -- calculates acceptable timeouts for tests by
analyzing their execution on
[swarming](https://github.com/luci/luci-py/tree/master/appengine/swarming).
* [manage.py](./manage.py) -- makes sure the buildbot configuration json is in
a standardized format.

## How the files are consumed
### Buildbot configuration json
Logic in the
[Chromium recipe](https://chromium.googlesource.com/chromium/tools/build/+/refs/heads/master/scripts/slave/recipes/chromium.py)
looks up each builder for each master and test generators in
[chromium_tests/steps.py](https://chromium.googlesource.com/chromium/tools/build/+/refs/heads/master/scripts/slave/recipe_modules/chromium_tests/steps.py)
parse the data. For example, as of
[a6e11220](https://chromium.googlesource.com/chromium/tools/build/+/a6e11220d97d578d6ba091abd68beba28a004722)
[generate_gtest](https://chromium.googlesource.com/chromium/tools/build/+/a6e11220d97d578d6ba091abd68beba28a004722/scripts/slave/recipe_modules/chromium_tests/steps.py#416)
parses any entry in a builder's
['gtest_tests'](https://chromium.googlesource.com/chromium/src/+/5750756522296b2a9a08009d8d2cc90db3b88f56/testing/buildbot/chromium.android.json#1243)
entry.

## Making changes

All of the JSON files in this directory are autogenerated. The "how to use"
section below describes the main tool, `generate_buildbot_json.py`, which
manages most of the waterfalls. It's no longer possible to hand-edit the JSON
files; presubmit checks forbid doing so.

Note that trybots mirror regular waterfall bots, with the mapping defined in
[trybots.py](https://chromium.googlesource.com/chromium/tools/build/+/refs/heads/master/scripts/slave/recipe_modules/chromium_tests/trybots.py).
This means that, as of
[81fcc4bc](https://chromium.googlesource.com/chromium/src/+/81fcc4bc6123ace8dd37db74fd2592e3e15ea46a/testing/buildbot/),
if you want to edit
[linux_android_rel_ng](https://chromium.googlesource.com/chromium/tools/build/+/59a2653d5f143213f4f166714657808b0c646bd7/scripts/slave/recipe_modules/chromium_tests/trybots.py#142),
you actually need to edit
[Android Tests](https://chromium.googlesource.com/chromium/src/+/81fcc4bc6123ace8dd37db74fd2592e3e15ea46a/testing/buildbot/chromium.linux.json#23).

### Trying the changes on trybots
You should be able to try build changes that affect the trybots directly (for
example, adding a test to linux_android_rel_ng should show up immediately in
your tryjob). Non-trybot changes have to be landed manually :(.

## Capacity considerations when editing the configuration files
When adding tests or bumping timeouts, care must be taken to ensure the
infrastructure has capacity to handle the extra load.  This is especially true
for the established
[Chromium CQ builders](https://chromium.googlesource.com/chromium/src/+/master/infra/config/branch/cq.cfg),
as they operate under strict execution requirements. Make sure to get an
infrastructure engineer on the Crossover Team to sign off that there is both
buildbot and swarming capacity available.

## How to use the generate_buildbot_json tool
### Test suites
#### Basic test suites

The [test_suites.pyl](./test_suites.pyl) file describes groups of tests that run
on bots -- both waterfalls and trybots. In order to specify that a test like
`base_unittests` runs on a bot, it must be put inside a test suite. This
organization helps enforce sharing of test suites among multiple bots.

An example of a simple test suite:

    'basic_chromium_gtests': {
      'base_unittests': {},
    }

If a bot in [waterfalls.pyl](./waterfalls.pyl) refers to the test suite
`basic_chromium_gtests`, then that bot will run `base_unittests`.

The test's name is usually both the build target as well as how the test appears
in the steps that the bot runs. However, this can be overridden using dictionary
arguments like `test` and `isolate_name`; see below.

The dictionary following the test's name can contain multiple entries that
affect how the test runs. Generally speaking, these are copied verbatim into the
generated JSON file. Commonly used arguments include:

* `args`: an array of command line arguments for the test.

* `swarming`: a dictionary of Swarming parameters. Note that these will be
  applied to *every* bot that refers to this test suite. It is often more useful
  to specify the Swarming dimensions at the bot level, in waterfalls.pyl. More
  on this below.

    * `can_use_on_swarming_builders`: if set to False, disables running this
      test on Swarming on any bot.

* `experiment_percentage`: an integer indicating that the test should be run
  as an experiment in the given percentage of builds. Tests running as
  experiments will not cause the containing builds to fail. Values should be
  in `[0, 100]` and will be clamped accordingly.

* `android_swarming`: Swarming parameters to be applied only on Android bots.
  (This feature was added mainly to match the original handwritten JSON files,
  and further use is discouraged. Ideally it should be removed.)

Arguments specific to GTest-based tests:

* `test`: the target to build and run, if different from the test's name. This
  allows the same test to be run multiple times on the same bot with different
  command line arguments or Swarming dimensions, for example.

Arguments specific to isolated script tests:

* `isolate_name`: the target to build and run, if different than the test's
  name.

There are other arguments specific to other test types (script tests, JUnit
tests, instrumentation tests, CTS tests); consult the generator script and
test_suites.pyl for more details and examples.

#### Composition test suites

One level of grouping of test suites is supported: composition test suites. A
composition test suite is an array whose contents must all be names of
individual test suites. Composition test suites *may not* refer to other
composition test suites. This restriction is by design. First, adding multiple
levels of indirection would make it more difficult to figure out which bots run
which tests. Second, having only one minimal grouping construct motivates
authors to simplify the configurations of tests on the bots and reduce the
number of test suites.

An example of a composition test suite:

    'common_gtests': {
      'base_unittests': {},
    },

    'linux_specific_gtests': {
      'x11_unittests': {},
    },

    # Composition test suite
    'linux_gtests': [
      'common_gtests',
      'linux_specific_gtests',
    ],

A bot referring to `linux_gtests` will run both `base_unittests` and
`x11_unittests`.

### Waterfalls

[waterfalls.pyl](./waterfalls.pyl) describes the waterfalls, the bots on those
waterfalls, and the test suites which those bots run.

A bot can specify a `swarming` dictionary including `dimension_sets`. These
parameters are applied to all tests that are run on this bot. Since most bots
run their tests on Swarming, this is one of the mechanisms that dramatically
reduces redundancy compared to maintaining the JSON files by hand.

A waterfall is a dictionary containing the following:

* `name`: the waterfall's name, for example `'chromium.win'`.
* `machines`: a dictionary mapping machine names to dictionaries containing bot
  descriptions.

Each bot's description is a dictionary containing the following:

* `additional_compile_targets`: if specified, an array of compile targets to
  build in addition to those for all of the tests that will run on this bot.

* `test_suites`: a dictionary optionally containing any of these kinds of
  tests. The value is a string referring either to a basic or composition test
  suite from [test_suites.pyl](./test_suites.pyl).

    * `cts_tests`: (Android-specific) conformance test suites.
    * `gtest_tests`: GTest-based tests.
    * `instrumentation_tests`: (Android-specific) instrumentation tests.
    * `isolated_scripts`: Isolated script tests. These are bundled into an
       isolate, invoke a wrapper script from src/testing/scripts as their
       top-level entry point, and are used to adapt to multiple kinds of test
       harnesses.
    * `junit_tests`: (Android-specific) JUnit tests.
    * `scripts`: Legacy script tests living in src/testing/scripts. These can
       not be Swarmed, and further use is discouraged.

* `swarming`: a dictionary specifying Swarming parameters to be applied to all
  tests that run on the bot.

* `os_type`: the type of OS this bot tests. The only useful value currently is
  `'android'`, and enables outputting of certain Android-specific entries into
  the JSON files.

* `skip_cipd_packages`: (Android-specific) when True, disables emission of the
  `'cipd_packages'` Swarming dictionary entry. Not commonly used; further use is
  discouraged.

* `skip_merge_script`: (Android-specific) when True, disables emission of the
  `'merge'` script key. Not commonly used; further use is discouraged.

* `skip_output_links`: (Android-specific) when True, disables emission of the
  `'output_links'` Swarming dictionary entry. Not commonly used; further use is
  discouraged.

* `use_swarming`: can be set to False to disable Swarming on a bot.

### Test suite exceptions

[test_suite_exceptions.pyl](./test_suite_exceptions.pyl) contains specific
exceptions to the general rules about which tests run on which bots described in
[test_suites.pyl](./test_suites.pyl) and [waterfalls.pyl](./waterfalls.pyl).

In general, the design should be to have no exceptions. Roughly speaking, all
bots should be treated identically, and ideally, the same set of tests should
run on each. In practice, of course, this is not possible.

The test suite exceptions can only be used to _remove tests from a bot_, _modify
how a test is run on a bot_, or _remove keys from a test&apos;s specification on
a bot_. The exceptions _can not_ be used to add a test to a bot. This
restriction is by design, and helps prevent taking shortcuts when designing test
suites which would make the test descriptions unmaintainable. (The number of
exceptions needed to describe Chromium's waterfalls in their previous
hand-maintained state has already gotten out of hand, and a concerted effort
should be made to eliminate them wherever possible.)

The exceptions file supports the following options per test:

* `remove_from`: a list of bot names on which this test should not run.
  Currently, bots on different waterfalls that have the same name can be
  disambiguated by appending the waterfall's name: for example, `Nougat Phone
  Tester chromium.android`.

* `modifications`: a dictionary mapping a bot's name to a dictionary of
  modifications that should be merged into the test's specification on that
  bot. This can be used to add additional command line arguments, Swarming
  parameters, etc.

* `replacements`: a dictionary mapping bot names to a dictionaries of field
  names to dictionaries of key/value pairs to replace. If the given value is
  `None`, then the key will simply be removed. For example:
  ```
  'foo_tests': {
    'Foo Tester': {
      'args': {
        '--some-flag': None,
        '--another-flag': 'some-value',
      },
    },
  }
  ```
  would remove the `--some-flag` and replace whatever value `--another-flag` was
  set to with `some-value`. Note that passing `None` only works if the flag
  being removed either has no value or is in the `--key=value` format. It does
  not work if the key and value are two separate entries in the args list.

### Order of application of test changes

A test's final JSON description comes from the following, in order:

* The dictionary specified in [test_suites.pyl](./test_suites.pyl). This is
  used as the starting point for the test's description on all bots.

* The specific bot's description in [waterfalls.pyl](./waterfalls.pyl). This
  dictionary is merged in to the test's dictionary. For example, the bot's
  Swarming parameters will override those specified for the test.

* Any exceptions specified per-bot in
  [test_suite_exceptions.pyl](./test_suite_exceptions.pyl). For example, any
  additional command line arguments will be merged in here. Any Swarming
  dictionary entries specified here will override both those specified in
  test_suites.pyl and waterfalls.pyl.

### Tips when making changes to the bot and test descriptions

In general, the only specialization of test suites that _should_ be necessary is
per operating system. If you add a new test to the bots and find yourself adding
lots of exceptions to exclude the test from bots all of one particular type
(like Android, Chrome OS, etc.), here are options to consider:

* Look for a different test suite to add it to -- such as one that runs
  everywhere except on that OS type.

* Add a new test suite that runs on all of the OS types where your new test
  should run, and add that test suite to the composition test suites referenced
  by the appropriate bots.

* Split one of the existing test suites into two, and add the newly created test
  suite (including your new test) to all of the bots except those which should
  not run the new test.

If adding a new waterfall, or a new bot to a waterfall, *please* avoid adding
new test suites. Instead, refer to one of the existing ones that is most similar
to the new bot(s) you are adding. There should be no need to continue
over-specializing the test suites.

If you see an opportunity to reduce redundancy or simplify test descriptions,
*please* consider making a contribution to the generate_buildbot_json script or
the data files. Some examples might include:

* Automatically doubling the number of shards on Debug bots, by describing to
  the tool which bots are debug bots. This could eliminate the need for a lot of
  exceptions.

* Specifying a single hard_timeout per bot, and eliminating all per-test
  timeouts from test_suites.pyl and test_suite_exceptions.pyl.

* Merging some test suites. When the generator tool was written, the handwritten
  JSON files were replicated essentially exactly. There are many opportunities
  to simplify the configuration of which tests run on which bots. For example,
  there's no reason why the top-of-tree Clang bots should run more tests than
  the bots on other waterfalls running the same OS.

`dpranke`, `jbudorick` or `kbr` will be glad to review any improvements you make
to the tools. Thanks in advance for contributing!
