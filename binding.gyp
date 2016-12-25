{
	"targets":
	[
	{
        "target_name": "configure",
        "type": "none",
        "actions": [ { "action_name": "js2c",
                       "inputs": [ "src/threads_a_gogo.cc" ],
                       "outputs": [ "src/threads_a_gogo.cc" ],
                       "action": ["bash", "-c", "cd <(module_root_dir)/src && node js2c.js"],
                       "message": "*** THREADS_A_GOGO: RUN JS2C" } ]
	},
	{
        "target_name": "threads_a_gogo",
        "sources":     [ "src/threads_a_gogo.cc" ],
        "cflags":      [ "-O3", "-pedantic" ],
        "cflags_c":    [ "-O3", "-pedantic" ],
        "cflags_cc":   [ "-O3", "-pedantic" ],
        # see https://gist.github.com/TooTallNate/1590684
        "xcode_settings": { "OTHER_CFLAGS": [ "-O3", "-pedantic" ] },
        "dependencies" : [ "configure" ]
	},
    {
        "target_name": "install",
        "type":"none",
        "dependencies" : [ "threads_a_gogo" ],
        "copies": [ {
            "destination": "<(module_root_dir)/node_modules",
            "files": ["<(module_root_dir)/build/Release/threads_a_gogo.node"]
        }]
    }]
}
