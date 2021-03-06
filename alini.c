/*
 * alini.c
 *
 * Copyright (c) 2003-2009 Denis Defreyne
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "alini.h"

/* strips whitespace at beginning and end of string */
static char *stripws(char *org_str, size_t len)
{
	int i;
	int begin = 0;
	int end = len - 1;
	char *str = NULL;

	if (!(str = strdup(org_str)))
	{
		return NULL;
	}

	while (isspace(str[begin]))
		begin++;

	while ((end >= begin) && isspace(str[end]))
		end--;

	// Shift all characters back to the start of the string array.
	for (i = begin; i <= end; i++)
		str[i - begin] = str[i];

	str[i - begin] = '\0'; // Null terminate string.

	return str;
}

/* create parser */
int alini_parser_create(alini_parser_t **parser, const char *path)
{
	int ret = 0;
	assert(parser);
	assert(path);

	if (!path)
		return -1;

	/* allocate new parser */
	*parser = (alini_parser_t *)calloc(1, sizeof(alini_parser_t));
	if(!(*parser))
	{
		return -1;
	}

	/* init */
	(*parser)->activesection = NULL;
	(*parser)->on = 1;

	/* copy path */
	if (!((*parser)->path = strdup(path)))
	{
		ret = -1;
		goto fail;
	}

	/* open file */
	(*parser)->file = fopen(path, "r");
	
	if(!(*parser)->file)
	{
		ret = 1; // Missing file is non-negative.
		goto fail;
	}

	return ret;
fail:
	alini_parser_dispose(*parser);
	*parser = NULL;

	return ret;
}

/* set `found key-value pair` callback */
int alini_parser_setcallback_foundkvpair(alini_parser_t *parser, alini_parser_foundkvpair_callback callback)
{
	assert(parser);
	
	/* set callback */
	parser->foundkvpair_callback = callback;
	
	return 0;
}

void alini_parser_set_context(alini_parser_t *parser, void *ctx)
{
	assert(parser);
	parser->ctx = ctx;
}

void *alini_parser_get_context(alini_parser_t *parser)
{
	assert(parser);
	return parser->ctx;
}

/* parse one step */
int alini_parser_step(alini_parser_t *parser)
{
	int 		ret 				= 0;
	char		line[4096];
	char		*tmpline			= NULL;
	unsigned	len 				= 0;
	char		signisfound			= 0;
	char		sectionhdrisfound	= 0;
	char		iswspace			= 0;
	unsigned	i;
	unsigned	j;
	char		*key				= NULL;
	char		*value				= NULL;
	
	assert(parser);

	parser->linenumber = 1;
	
	while(1)
	{
		/* get a line */
		if(!fgets(line, sizeof(line) - 1, parser->file))
		{
			ret = 1; goto fail; // EOF reached.
		}

		parser->linenumber++;
		
		/* skip comments and empty lines */
		if(line[0] == '#' || line[0] == ';' || line[0] == '\n' || line[0] == '\r')
			continue;
		
		/* skip lines containing whitespace */
		len = strlen(line);
		iswspace = 1;
		for(j = 0 ; j < len && iswspace; j++)
		{
			if(!isspace(line[j]) && line[j] !='\n' && line[j] !='\r')
				iswspace = 0;
		}
		if(iswspace) continue;
		
		/* search '[...]' */
		sectionhdrisfound = 0;
		if (!(tmpline = stripws(line, strlen(line))))
		{
			ret = -1; goto fail;
		}

		len = strlen(tmpline);
		if(len > 2)
		{
			if(tmpline[0] == '[')
			{
				if(tmpline[len-1] == ']')
				{
					sectionhdrisfound = 1;
					if(parser->activesection) free(parser->activesection);
					
					if (!(parser->activesection = stripws(tmpline + 1, strlen(tmpline) - 2)))
					{
						ret = -1; goto fail;
					}
				}
				else
				{
					fprintf(stderr,
						"alini: parse error at %s:%d: end token `]' not found",
						parser->path, parser->linenumber);
					ret = -1; goto fail;
				}
			}
		}
		free(tmpline);
		tmpline = NULL;
		
		if(!sectionhdrisfound)
		{
			char *signpos = NULL;

			if ((signpos = strchr(line, '=')) == NULL)
			{
				fprintf(stderr,
					"alini: parse error at %s:%d: token `=' not found",
					parser->path, parser->linenumber);
				ret = -1; goto fail;
			}

			i = signpos - line;
			
			/* trim key and value */
			if (!(key = stripws(line, i)))
			{
				ret = -1; goto fail;
			}

			i++;
			
			if (!(value = stripws(line + i, strlen(line) - i)))
			{
				ret = -1; goto fail;
			}

			/* call callback */
			parser->foundkvpair_callback(parser, parser->activesection, key, value);
			
			/* cleanup */
			free(key);
			key = NULL;
			free(value);
			value = NULL;
			
			break;
		}
	}
	
	return ret;
fail:
	if (tmpline) free(tmpline);
	if (key) free(key);
	if (value) free(value);

	return ret;
}

/* parse entire file */
int alini_parser_start(alini_parser_t *parser)
{
	int ret = 0;
	assert(parser);
	
	while (parser->on && (ret == 0))
	{
		ret = alini_parser_step(parser);

		if (ret < 0)
			return -1;
	}
	
	return 0;
}

/* halt parser */
void alini_parser_halt(alini_parser_t *parser)
{
	assert(parser);
	
	parser->on = 0;
}

/* dispose parser */
int alini_parser_dispose(alini_parser_t *parser)
{
	if (!parser)
		return 0;
	
	/* close file */
	if (parser->file)
		fclose(parser->file);

	if (parser->path)
		free(parser->path);
	
	/* free parser */
	free(parser);
	
	return 0;
}

int alini_parser_get_linenumber(alini_parser_t *parser)
{
	assert(parser);
	return parser->linenumber;
}
