{
	"targets":
	[{
        "target_name": "threads_a_gogo",
        "sources": [ "src/threads_a_gogo.cc" ]
	},
    {
        "target_name": "install",
        "type":"none",
        "dependencies" : [ "threads_a_gogo" ],
        "copies": [
        {
            "destination": "<(module_root_dir)/node_modules",
            "files": ["<(module_root_dir)/build/Release/threads_a_gogo.node"]
        }]
    }]
}
