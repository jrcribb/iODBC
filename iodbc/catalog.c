/*
 *  catalog.c
 *
 *  $Id$
 *
 *  Catalog functions
 *
 *  The iODBC driver manager.
 *  
 *  Copyright (C) 1995 by Ke Jin <kejin@empress.com> 
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>

#include <sql.h>
#include <sqlext.h>


#include <herr.h>
#include <henv.h>
#include <hdbc.h>
#include <hstmt.h>

#include <dlproc.h>
#include <itrace.h>

/* 
 *  Check state for executing catalog functions 
 */
static SQLRETURN
_iodbcdm_cata_state_ok (
    STMT_t FAR * pstmt,
    int fidx)
{
  int sqlstat = en_00000;

  if (pstmt->asyn_on == en_NullProc)
    {
      switch (pstmt->state)
	{
	case en_stmt_needdata:
	case en_stmt_mustput:
	case en_stmt_canput:
	  sqlstat = en_S1010;
	  break;

	case en_stmt_fetched:
	case en_stmt_xfetched:
	  sqlstat = en_24000;
	  break;

	default:
	  break;
	}
    }
  else if (pstmt->asyn_on != fidx)
    {
      sqlstat = en_S1010;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      return SQL_ERROR;
    }

  return SQL_SUCCESS;
}


/* 
 *  State transition for catalog function 
 */
static SQLRETURN
_iodbcdm_cata_state_tr (
    STMT_t FAR * pstmt,
    int fidx,
    SQLRETURN result)
{
  CONN (pdbc, pstmt->hdbc);

  if (pstmt->asyn_on == fidx)
    {
      switch (result)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	case SQL_ERROR:
	  pstmt->asyn_on = en_NullProc;
	  break;

	case SQL_STILL_EXECUTING:
	default:
	  return result;
	}
    }

  if (pstmt->state <= en_stmt_executed)
    {
      switch (result)
	{
	case SQL_SUCCESS:
	case SQL_SUCCESS_WITH_INFO:
	  pstmt->state = en_stmt_cursoropen;
	  break;

	case SQL_ERROR:
	  pstmt->state = en_stmt_allocated;
	  pstmt->prep_state = 0;
	  break;

	case SQL_STILL_EXECUTING:
	  pstmt->asyn_on = fidx;
	  break;

	default:
	  break;
	}
    }

  return result;
}


SQLRETURN SQL_API
SQLGetTypeInfo (
    SQLHSTMT hstmt,
    SQLSMALLINT fSqlType)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  int sqlstat = en_00000;
  SQLRETURN retcode;

  ENTER_STMT (pstmt);

  for (;;)
    {
#if (ODBCVER < 0x0300)
      if (fSqlType > SQL_TYPE_MAX)
	{
	  sqlstat = en_S1004;
	  break;
	}

      /* Note: SQL_TYPE_DRIVER_START is a negative number So, we use ">" */
      if (fSqlType < SQL_TYPE_MIN && fSqlType > SQL_TYPE_DRIVER_START)
	{
	  sqlstat = en_S1004;
	  break;
	}
#endif	/* ODBCVER < 0x0300 */

      retcode = _iodbcdm_cata_state_ok (pstmt, en_GetTypeInfo);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_GetTypeInfo);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;
      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_GetTypeInfo,
      (pstmt->dhstmt, fSqlType));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_GetTypeInfo, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLSpecialColumns (
    SQLHSTMT hstmt,
    SQLUSMALLINT fColType,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLUSMALLINT fScope,
    SQLUSMALLINT fNullable)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      if (fColType != SQL_BEST_ROWID && fColType != SQL_ROWVER)
	{
	  sqlstat = en_S1097;
	  break;
	}

      if (fScope != SQL_SCOPE_CURROW
	  && fScope != SQL_SCOPE_TRANSACTION
	  && fScope != SQL_SCOPE_SESSION)
	{
	  sqlstat = en_S1098;
	  break;
	}

      if (fNullable != SQL_NO_NULLS && fNullable != SQL_NULLABLE)
	{
	  sqlstat = en_S1099;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_SpecialColumns);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_SpecialColumns);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;
      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_SpecialColumns, (
	  pstmt->dhstmt,
	  fColType,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName,
	  fScope,
	  fNullable));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_SpecialColumns, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLStatistics (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLUSMALLINT fUnique,
    SQLUSMALLINT fAccuracy)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      if (fUnique != SQL_INDEX_UNIQUE && fUnique != SQL_INDEX_ALL)
	{
	  sqlstat = en_S1100;
	  break;
	}

      if (fAccuracy != SQL_ENSURE && fAccuracy != SQL_QUICK)
	{
	  sqlstat = en_S1101;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_Statistics);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_Statistics);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_Statistics, (
	  pstmt->dhstmt,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName,
	  fUnique,
	  fAccuracy));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_Statistics, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLTables (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLCHAR FAR * szTableType,
    SQLSMALLINT cbTableType)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS)
	  || (cbTableType < 0 && cbTableType != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_Tables);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_Tables);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_Tables, (
	  pstmt->dhstmt,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName,
	  szTableType,
	  cbTableType));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_Tables, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLColumnPrivileges (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLCHAR FAR * szColumnName,
    SQLSMALLINT cbColumnName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS)
	  || (cbColumnName < 0 && cbColumnName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_ColumnPrivileges);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_ColumnPrivileges);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_ColumnPrivileges, (
	  pstmt->dhstmt,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName,
	  szColumnName,
	  cbColumnName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_ColumnPrivileges, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLColumns (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName,
    SQLCHAR FAR * szColumnName,
    SQLSMALLINT cbColumnName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS)
	  || (cbColumnName < 0 && cbColumnName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_Columns);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_Columns);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_Columns, (
	  pstmt->dhstmt,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName,
	  szColumnName,
	  cbColumnName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_Columns, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLForeignKeys (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szPkTableQualifier,
    SQLSMALLINT cbPkTableQualifier,
    SQLCHAR FAR * szPkTableOwner,
    SQLSMALLINT cbPkTableOwner,
    SQLCHAR FAR * szPkTableName,
    SQLSMALLINT cbPkTableName,
    SQLCHAR FAR * szFkTableQualifier,
    SQLSMALLINT cbFkTableQualifier,
    SQLCHAR FAR * szFkTableOwner,
    SQLSMALLINT cbFkTableOwner,
    SQLCHAR FAR * szFkTableName,
    SQLSMALLINT cbFkTableName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbPkTableQualifier < 0 && cbPkTableQualifier != SQL_NTS)
	  || (cbPkTableOwner < 0 && cbPkTableOwner != SQL_NTS)
	  || (cbPkTableName < 0 && cbPkTableName != SQL_NTS)
	  || (cbFkTableQualifier < 0 && cbFkTableQualifier != SQL_NTS)
	  || (cbFkTableOwner < 0 && cbFkTableOwner != SQL_NTS)
	  || (cbFkTableName < 0 && cbFkTableName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_ForeignKeys);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_ForeignKeys);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_ForeignKeys, (
	  pstmt->dhstmt,
	  szPkTableQualifier,
	  cbPkTableQualifier,
	  szPkTableOwner,
	  cbPkTableOwner,
	  szPkTableName,
	  cbPkTableName,
	  szFkTableQualifier,
	  cbFkTableQualifier,
	  szFkTableOwner,
	  cbFkTableOwner,
	  szFkTableName,
	  cbFkTableName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_ForeignKeys, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLPrimaryKeys (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_PrimaryKeys);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_PrimaryKeys);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_PrimaryKeys, (
	  pstmt->dhstmt,
	  szTableQualifier,
	  cbTableQualifier,
	  szTableOwner,
	  cbTableOwner,
	  szTableName,
	  cbTableName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_PrimaryKeys, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLProcedureColumns (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szProcQualifier,
    SQLSMALLINT cbProcQualifier,
    SQLCHAR FAR * szProcOwner,
    SQLSMALLINT cbProcOwner,
    SQLCHAR FAR * szProcName,
    SQLSMALLINT cbProcName,
    SQLCHAR FAR * szColumnName,
    SQLSMALLINT cbColumnName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbProcQualifier < 0 && cbProcQualifier != SQL_NTS)
	  || (cbProcOwner < 0 && cbProcOwner != SQL_NTS)
	  || (cbProcName < 0 && cbProcName != SQL_NTS)
	  || (cbColumnName < 0 && cbColumnName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_ProcedureColumns);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_ProcedureColumns);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_ProcedureColumns, (
	  pstmt->dhstmt,
	  szProcQualifier,
	  cbProcQualifier,
	  szProcOwner,
	  cbProcOwner,
	  szProcName,
	  cbProcName,
	  szColumnName,
	  cbColumnName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_ProcedureColumns, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLProcedures (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szProcQualifier,
    SQLSMALLINT cbProcQualifier,
    SQLCHAR FAR * szProcOwner,
    SQLSMALLINT cbProcOwner,
    SQLCHAR FAR * szProcName,
    SQLSMALLINT cbProcName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbProcQualifier < 0 && cbProcQualifier != SQL_NTS)
	  || (cbProcOwner < 0 && cbProcOwner != SQL_NTS)
	  || (cbProcName < 0 && cbProcName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_Procedures);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_Procedures);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_Procedures, (
	  pstmt->dhstmt,
	  szProcQualifier,
	  cbProcQualifier,
	  szProcOwner,
	  cbProcOwner,
	  szProcName,
	  cbProcName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_Procedures, retcode);
  LEAVE_STMT (pstmt, retcode);
}


SQLRETURN SQL_API
SQLTablePrivileges (
    SQLHSTMT hstmt,
    SQLCHAR FAR * szTableQualifier,
    SQLSMALLINT cbTableQualifier,
    SQLCHAR FAR * szTableOwner,
    SQLSMALLINT cbTableOwner,
    SQLCHAR FAR * szTableName,
    SQLSMALLINT cbTableName)
{
  STMT (pstmt, hstmt);
  HPROC hproc = SQL_NULL_HPROC;
  SQLRETURN retcode;
  int sqlstat = en_00000;

  ENTER_STMT (pstmt);

  for (;;)
    {
      if ((cbTableQualifier < 0 && cbTableQualifier != SQL_NTS)
	  || (cbTableOwner < 0 && cbTableOwner != SQL_NTS)
	  || (cbTableName < 0 && cbTableName != SQL_NTS))
	{
	  sqlstat = en_S1090;
	  break;
	}

      retcode = _iodbcdm_cata_state_ok (pstmt, en_TablePrivileges);

      if (retcode != SQL_SUCCESS)
	{
	  LEAVE_STMT (pstmt, SQL_ERROR);
	}

      hproc = _iodbcdm_getproc (pstmt->hdbc, en_TablePrivileges);

      if (hproc == SQL_NULL_HPROC)
	{
	  sqlstat = en_IM001;
	  break;
	}

      sqlstat = en_00000;

      if (1)			/* turn off solaris warning message */
	break;
    }

  if (sqlstat != en_00000)
    {
      PUSHSQLERR (pstmt->herr, sqlstat);

      LEAVE_STMT (pstmt, SQL_ERROR);
    }

  CALL_DRIVER (pstmt->hdbc, pstmt, retcode, hproc, en_TablePrivileges,
      (pstmt->dhstmt, szTableQualifier, cbTableQualifier, szTableOwner,
	  cbTableOwner, szTableName, cbTableName));

  retcode = _iodbcdm_cata_state_tr (pstmt, en_TablePrivileges, retcode);
  LEAVE_STMT (pstmt, retcode);
}