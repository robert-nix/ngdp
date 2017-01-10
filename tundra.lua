Build {
	Units = function ()
		local ngdp = Program {
			Name = "ngdp",
			Sources = {
				"Client.h",
				"Client.cpp",
				"Remote.h",
				"Remote.cpp",
				"Key.h",
				"Key.cpp",
				"Config.h",
				"Config.cpp",

				"main.cpp",

				"ngdp.h",

				"Heap.h",
				"FileIO.h",
				"Strings.h",
				"Buffer.h",
				"std.h",

				"Containers.natvis",
			},
			Libs = {
				{
					"lib/libcurl_a_debug.lib";
					Config = "win32-vs2015-debug"
				},
				{
					"lib/libcurl_a.lib";
					Config = "win32-vs2015-release"
				},
			},
		}

		Default "ngdp"
	end,

	Env = {
		CXXOPTS = {
			"/W4 /guard:cf- /fp:fast /Gm- /Gy",
			{ "/O2 /MT"; Config = "*-vs2015-release" },
			{ "/MTd"; Config = "*-vs2015-debug" },
		},
		ASMOPTS = {
			{ "-f win32"; Config = "win32-vs2015-*" },
		},
		GENERATE_PDB = "1",
		CPPPATH = {
			".",
			"include",
		},
		CPPDEFS = {
			"CURL_STATICLIB",
			{ "NDEBUG"; Config = "*-vs2015-release" },
		},
		PROGOPTS = {
			{ "/INCREMENTAL:NO /OPT:REF"; Config = "*-vs2015-release" },
		},
		SHLIBOPTS = {
			{ "/INCREMENTAL:NO /OPT:REF"; Config = "*-vs2015-release" },
		},
		-- Make build ignore .natvis files like it ignores .h files:
		IGNORED_AUTOEXTS = {
			".natvis"
		},
	},

	Configs = {
		Config {
			Name = "win32-vs2015",
			Tools = { { "msvc-vs2015"; TargetArch = "x86" } },
			SupportedHosts = { "windows" },
		},
	},

	IdeGenerationHints = {
		Msvc = {
			-- Remap config names to MSVC platform names (affects things like header scanning & debugging)
			PlatformMappings = {
				["win32-vs2015"] = "Win32",
			},
			-- Remap variant names to MSVC friendly names
			VariantMappings = {
				["release"]    = "Release",
				["debug"]      = "Debug",
				["production"] = "Production",
			},
		},

		-- Override output directory for sln/vcxproj files.
		MsvcSolutionDir = "vs2015",

		-- Override solutions to generate and what units to put where.
		MsvcSolutions = {
			["ngdp.sln"] = {},
		},

		-- Cause all projects to have "Build" ticked on them inside the MSVC Configuration Manager.
		-- As a result of this, you can choose a project as the "Startup Project",
		-- and when hitting "Debug" or "Run", the IDE will build that project before running it.
		-- You will want to avoid pressing "Build Solution" with this option turned on, because MSVC
		-- will kick off all project builds simultaneously.
		BuildAllByDefault = true,
	}
}
