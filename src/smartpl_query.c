/*
 * Copyright (C) 2018 Christian Meffert <christian.meffert@googlemail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "smartpl_query.h"
#include "logger.h"
#include "misc.h"

#include "SMARTPLLexer.h"
#include "SMARTPLParser.h"
#include "SMARTPL2SQL.h"


static int
parse_input(struct smartpl *smartpl, pANTLR3_INPUT_STREAM input)
{
  pSMARTPLLexer lxr;
  pANTLR3_COMMON_TOKEN_STREAM tstream;
  pSMARTPLParser psr;
  SMARTPLParser_playlist_return qtree;
  pANTLR3_COMMON_TREE_NODE_STREAM nodes;
  pSMARTPL2SQL sqlconv;
  SMARTPL2SQL_playlist_return plreturn;
  int ret;

  lxr = SMARTPLLexerNew(input);

  // Need to check for errors
  if (lxr == NULL)
    {
      DPRINTF(E_LOG, L_SCAN, "Could not create SMARTPL lexer\n");
      ret = -1;
      goto lxr_fail;
    }

  tstream = antlr3CommonTokenStreamSourceNew(ANTLR3_SIZE_HINT, TOKENSOURCE(lxr));

  if (tstream == NULL)
    {
      DPRINTF(E_LOG, L_SCAN, "Could not create SMARTPL token stream\n");
      ret = -1;
      goto tkstream_fail;
    }

  // Finally, now that we have our lexer constructed, we can create the parser
  psr = SMARTPLParserNew(tstream);  // CParserNew is generated by ANTLR3

  if (psr == NULL)
    {
      DPRINTF(E_LOG, L_SCAN, "Could not create SMARTPL parser\n");
      ret = -1;
      goto psr_fail;
    }

  qtree = psr->playlist(psr);

  /* Check for parser errors */
  if (psr->pParser->rec->state->errorCount > 0)
    {
      DPRINTF(E_LOG, L_SCAN, "SMARTPL query parser terminated with %d errors\n", psr->pParser->rec->state->errorCount);
      ret = -1;
      goto psr_error;
    }

  DPRINTF(E_DBG, L_SCAN, "SMARTPL query AST:\n\t%s\n", qtree.tree->toStringTree(qtree.tree)->chars);

  nodes = antlr3CommonTreeNodeStreamNewTree(qtree.tree, ANTLR3_SIZE_HINT);
  if (!nodes)
    {
      DPRINTF(E_LOG, L_SCAN, "Could not create node stream\n");
      ret = -1;
      goto psr_error;
    }

  sqlconv = SMARTPL2SQLNew(nodes);
  if (!sqlconv)
    {
      DPRINTF(E_LOG, L_SCAN, "Could not create SQL converter\n");
      ret = -1;
      goto sql_fail;
    }

  plreturn = sqlconv->playlist(sqlconv);

  /* Check for tree parser errors */
  if (sqlconv->pTreeParser->rec->state->errorCount > 0)
    {
      DPRINTF(E_LOG, L_SCAN, "SMARTPL query tree parser terminated with %d errors\n", sqlconv->pTreeParser->rec->state->errorCount);
      ret = -1;
      goto sql_error;
    }

  if (plreturn.title && plreturn.query)
    {
      DPRINTF(E_DBG, L_SCAN, "SMARTPL SQL title '%s', query: '%s', having: '%s', order by: '%s', limit: %d \n", plreturn.title->chars, plreturn.query->chars, plreturn.having->chars, plreturn.orderby->chars, plreturn.limit);

      if (smartpl->title)
	free(smartpl->title);
      smartpl->title = strdup((char *)plreturn.title->chars);

      if (smartpl->query_where)
	free(smartpl->query_where);
      smartpl->query_where = strdup((char *)plreturn.query->chars);

      if (smartpl->having)
	free(smartpl->having);
      smartpl->having = safe_strdup((char *)plreturn.having->chars);

      if (smartpl->order)
	free(smartpl->order);
      smartpl->order = safe_strdup((char *)plreturn.orderby->chars);

      smartpl->limit = plreturn.limit;

      ret = 0;
    }
  else
    {
      DPRINTF(E_LOG, L_SCAN, "Invalid SMARTPL query\n");
      ret = -1;
    }

 sql_error:
  sqlconv->free(sqlconv);
 sql_fail:
  nodes->free(nodes);
 psr_error:
  psr->free(psr);
 psr_fail:
  tstream->free(tstream);
 tkstream_fail:
  lxr->free(lxr);
 lxr_fail:

  return ret;
}

int
smartpl_query_parse_file(struct smartpl *smartpl, const char *file)
{
  pANTLR3_INPUT_STREAM input;
  int ret;

#if ANTLR3C_NEW_INPUT
  input = antlr3FileStreamNew((pANTLR3_UINT8) file, ANTLR3_ENC_8BIT);
#else
  input = antlr3AsciiFileStreamNew((pANTLR3_UINT8) file);
#endif

  // The input will be created successfully, providing that there is enough memory and the file exists etc
  if (input == NULL)
    {
      DPRINTF(E_LOG, L_SCAN, "Unable to open smart playlist file %s\n", file);
      return -1;
    }

  ret = parse_input(smartpl, input);
  input->close(input);

  return ret;
}

int
smartpl_query_parse_string(struct smartpl *smartpl, const char *expression)
{
  pANTLR3_INPUT_STREAM input;
  int ret;

#if ANTLR3C_NEW_INPUT
  input = antlr3StringStreamNew ((pANTLR3_UINT8)expression, ANTLR3_ENC_8BIT, (ANTLR3_UINT64)strlen(expression), (pANTLR3_UINT8)"SMARTPL expression");
#else
  input = antlr3NewAsciiStringInPlaceStream ((pANTLR3_UINT8)expression, (ANTLR3_UINT64)strlen(expression), (pANTLR3_UINT8)"SMARTPL expression");
#endif

  // The input will be created successfully, providing that there is enough memory and the file exists etc
  if (input == NULL)
    {
      DPRINTF(E_LOG, L_SCAN, "Unable to pars smart pl expression %s\n", expression);
      return -1;
    }

  ret = parse_input(smartpl, input);
  input->close(input);

  return ret;
}


void
free_smartpl(struct smartpl *smartpl, int content_only)
{
  if (!smartpl)
    return;

  free(smartpl->title);
  free(smartpl->query_where);
  free(smartpl->having);
  free(smartpl->order);

  if (!content_only)
    free(smartpl);
  else
    memset(smartpl, 0, sizeof(struct smartpl));
}

