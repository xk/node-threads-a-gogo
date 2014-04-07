{
	"targets": [
		{
			"target_name": "threads_a_gogo",
			#"requires": ["minifier"], |> We dont need this, as the script files are pre-made already.
			"sources": [
				"src/threads_a_gogo.cc",
				# Generated
				"src/createPool.js.c",
				"src/events.js.c",
				"src/load.js.c",
				"src/thread_nextTick.js.c"
			]
		}#,{}
	]
}
