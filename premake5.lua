local target_suffixes_86 = {
	bsd       = "_linux",
	linux     = "_linux",
	solaris   = "_linux",
	windows   = "_win32",
	macosx    = "_osx"  ,
}

local target_suffixes_64 = {
	bsd       = "_linux64",
	linux     = "_linux64",
	solaris   = "_linux64",
	windows   = "_win64",
	macosx    = "_osx64",
}

local libdirs_86 = {
	linux     = "steamworks/lib/linux32",
	windows   = "steamworks/lib/win32"
}

local libdirs_64 = {
	linux     = "steamworks/lib/linux64",
	windows   = "steamworks/lib/win64"
}

solution "steamhttp"
	language		"C++"
	location		"project"

	--
	-- Statically link the C-Runtime to reduce dependencies needed to run our module
	--
	staticruntime "On"

	configurations { "Debug", "Release" }
	platforms { "x86", "x64" }

	configuration "Debug"
		defines { "DEBUG_BUILD" }
		symbols		"On"		-- Generate debugging information
		optimize	"On"		-- Optimize the build output for size and speed

	configuration "Release"
		defines { "RELEASE_BUILD" }
		optimize	"On"		-- Optimize the build output for size and speed

	filter "platforms:x86"
		architecture "x86"
		targetsuffix ( target_suffixes_86[os.target()] )

	filter "platforms:x64"
		architecture "x86_64"
		targetsuffix ( target_suffixes_64[os.target()] )

	project "steamhttp"
		kind	"SharedLib"
		targetprefix "gmsv_"
		targetextension ".dll"
		includedirs { "steamworks/include/", "gmod-module-base/include/" }
		files { "src/*.cpp", "src/*.h", "src/"..os.target().."/*.cpp", "src/"..os.target().."/*.h" }

		filter "platforms:x64"
			libdirs {libdirs_64[os.target()]}
		filter "platforms:x86"
			libdirs {libdirs_86[os.target()]}


		if os.target() == "windows" then
		    defines { "WINDOWS_BUILD" }
			filter "platforms:x64"
				links {"steam_api64"}
			filter "platforms:x86"
				links {"steam_api"}
		else
			links {"steam_api"}
		end

