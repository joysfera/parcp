/*
**  CFGOPTS.H
*/

#ifndef TAG_TYPE_defined
#define TAG_TYPE_defined
typedef enum {
	Error_Tag,
	Byte_Tag,
	Boolean_Tag,
	Word_Tag,
	Int_Tag,
	Long_Tag,
	HexWord_Tag,
	HexInt_Tag,
	HexLong_Tag,
	Float_Tag,
	Double_Tag,
	Char_Tag,
	String_Tag,
	Function_Tag
} TAG_TYPE;

struct Config_Tag {
	char		*code;				/* Option switch        */
	TAG_TYPE	type;				/* Type of option       */
	void		 *buf;				/* Storage location     */
	short		buf_size;			/* Storage size for String_Tag - max. 32k */
	char        stat;				/* internal flag for update_config */
};

int input_config(const char *, struct Config_Tag *, char *);
int update_config(const char *, struct Config_Tag *, char *);
#endif
