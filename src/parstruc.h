struct Config_Tag mconfigs[] = {
#ifdef IBM
#  ifdef USB
	{ "UsbSerial", String_Tag, usb_serial, sizeof(usb_serial) },
#  else
	{ CFG_PORT, HexInt_Tag, &print_port },
	{ CFG_UNIBI, Boolean_Tag, &PCunidirect },
	{ "PortType", Int_Tag, &port_type },
	{ "CableType", Int_Tag, &cable_type },
#  endif
#endif
	{ "FastRoutines", Boolean_Tag, &_assembler },
	{ "ProcessSubDir", Boolean_Tag, &_send_subdir },
	{ "CaseSensitive", Boolean_Tag, &_case_sensitive },
	{ "PreserveCase", Boolean_Tag, &_preserve_case },
	{ "ShowHidden", Boolean_Tag, &_show_hidden },
	{ "OverOlder", Char_Tag, &_over_older },
	{ "OverNewer", Char_Tag, &_over_newer },
	{ "KeepTimeStamp", Boolean_Tag, &_keep_timestamp },
	{ "KeepAttribs", Boolean_Tag, &_keep_attribs },
	{ "HashMark", Boolean_Tag, &hash_mark },
	{ "QuitAfterDrop", Boolean_Tag, &afterdrop },
	{ "CRC", Boolean_Tag, &_checksum },
	{ "BlockSize", Long_Tag, &buffer_lenkb },
	{ "DirectoryLines", Word_Tag, &dirbuf_lines },
	{ "FileBuffers", Byte_Tag, &filebuffers },
	{ "Timeout", Int_Tag, &time_out },
	{ "DirSort", Char_Tag, &_sort_jak },
	{ "SortCase", Boolean_Tag, &_sort_case },
	{ "Autoexec", String_Tag, autoexec, sizeof(autoexec) },
	{ "ArchiveMode", Boolean_Tag, &_archive_mode },
	{ "CollectInfo", Boolean_Tag, &_check_info },

#ifdef SHELL
	{ "Shell", Boolean_Tag, &shell },
#endif

#ifdef DEBUG
	{ "Debug", Word_Tag, &debuglevel },
	{ "LogFile", String_Tag, logfile, sizeof(logfile) },
	{ "NoLog", String_Tag, nolog_str, sizeof(nolog_str) },
	{ "NoDisplay", String_Tag, nodisp_str, sizeof(nodisp_str) },
#endif
	{ NULL , Error_Tag, NULL        }
};
