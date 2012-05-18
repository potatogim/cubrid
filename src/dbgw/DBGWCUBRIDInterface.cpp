/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include "DBGWCommon.h"
#include "DBGWError.h"
#include "DBGWValue.h"
#include "DBGWLogger.h"
#include "DBGWQuery.h"
#include "DBGWDataBaseInterface.h"
#include "DBGWCUBRIDInterface.h"

namespace dbgw
{

  namespace db
  {

    int convertValueTypeToCCIAType(DBGWValueType type)
    {
      switch (type)
        {
        case DBGW_VAL_TYPE_STRING:
        case DBGW_VAL_TYPE_CHAR:
          return CCI_A_TYPE_STR;
        case DBGW_VAL_TYPE_INT:
          return CCI_A_TYPE_INT;
        case DBGW_VAL_TYPE_LONG:
          return CCI_A_TYPE_BIGINT;
        default:
          InvalidValueTypeException e(type);
          DBGW_LOG_ERROR(e.what());
          throw e;
        }
    }

    DBGWValueType convertCCIUTypeToValueType(int utype)
    {
      switch (utype)
        {
        case CCI_U_TYPE_CHAR:
          return DBGW_VAL_TYPE_CHAR;
        case CCI_U_TYPE_INT:
        case CCI_U_TYPE_SHORT:
          return DBGW_VAL_TYPE_INT;
        case CCI_U_TYPE_BIGINT:
          return DBGW_VAL_TYPE_LONG;
        case CCI_U_TYPE_STRING:
        case CCI_U_TYPE_NCHAR:
        case CCI_U_TYPE_VARNCHAR:
          return DBGW_VAL_TYPE_STRING;
        default:
          DBGW_LOG_WARN((boost::
              format
              ("%d type is not yet supported. so converted string.")
              % utype).str().c_str());
          return DBGW_VAL_TYPE_STRING;
        }
    }

    CUBRIDException::CUBRIDException(const string &errorMessage) throw() :
      DBGWInterfaceException(DBGWErrorCode::NO_ERROR, errorMessage),
      m_bCCIError(false), m_pError(NULL)
    {
      createErrorMessage();
    }

    CUBRIDException::CUBRIDException(int nInterfaceErrorCode, const string &replace) throw() :
      DBGWInterfaceException(nInterfaceErrorCode), m_bCCIError(true),
      m_replace(replace), m_pError(NULL)
    {
      createErrorMessage();
    }

    CUBRIDException::CUBRIDException(int nInterfaceErrorCode, T_CCI_ERROR &error,
        const string &replace) throw() :
      DBGWInterfaceException(nInterfaceErrorCode), m_bCCIError(true),
      m_replace(replace), m_pError(&error)
    {
      createErrorMessage();
    }

    void CUBRIDException::doCreateErrorMessage()
    {
      stringstream buffer;
      buffer << "[" << m_nErrorCode << "]";

      if (m_bCCIError)
        {
          if (m_pError != NULL && m_pError->err_code != DBGWErrorCode::NO_ERROR)
            {
              m_nInterfaceErrorCode = m_pError->err_code;
              m_errorMessage = m_pError->err_msg;
            }

          if (m_errorMessage == "")
            {
              char szBuffer[100];
              if (cci_get_err_msg(m_nInterfaceErrorCode, szBuffer, 100) == 0)
                {
                  m_errorMessage = szBuffer;
                }
              else
                {
                  m_errorMessage = m_replace;
                }
            }
        }

      buffer << "[" << m_nInterfaceErrorCode << "]";
      buffer << " " << m_errorMessage;
      m_what = buffer.str();
    }

    CUBRIDException::~CUBRIDException() throw()
    {
    }

    DBGWCUBRIDConnection::DBGWCUBRIDConnection(const string &groupName,
        const string &host, int nPort, const DBGWDBInfoHashMap &dbInfoMap) :
      DBGWConnection(groupName, host, nPort, dbInfoMap), m_bClosed(false),
      m_hCCIConnection(-1)
    {
      /**
       * We don't need to clear error because this api will not make error.
       *
       * clearException();
       *
       * try
       * {
       * 		blur blur blur;
       * }
       * catch (DBGWException &e)
       * {
       * 		setLastException(e);
       * }
       */
    }

    DBGWCUBRIDConnection::~DBGWCUBRIDConnection()
    {
      clearException();

      try
        {
          if (close() < 0)
            {
              throw getLastException();
            }
        }
      catch (DBGWException &e)
        {
          setLastException(e);
        }
    }

    bool DBGWCUBRIDConnection::connect()
    {
      clearException();

      try
        {
          DBGWDBInfoHashMap dbInfoMap = getDBInfoMap();
          DBGWDBInfoHashMap::const_iterator cit = dbInfoMap.find("dbname");
          if (cit == dbInfoMap.end())
            {
              CUBRIDException e(
                  "Not exist required property in dataabse info map.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }

          cit = dbInfoMap.find("dbuser");
          if (cit == dbInfoMap.end())
            {
              CUBRIDException e(
                  "Not exist required property in dataabse info map.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }

          cit = dbInfoMap.find("dbpasswd");
          if (cit == dbInfoMap.end())
            {
              CUBRIDException e(
                  "Not exist required property in dataabse info map.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }

          stringstream connectionUrl;
          connectionUrl << "cci:CUBRID:" << getHost() << ":" << getPort() << ":"
              << dbInfoMap["dbname"] << ":" << dbInfoMap["dbuser"] << ":"
              << dbInfoMap["dbpasswd"] << ":";

          cit = dbInfoMap.find("althosts");
          if (cit == dbInfoMap.end())
            {
              connectionUrl << "?";
            }
          else
            {
              connectionUrl << ":" << dbInfoMap["althosts"] << "&";
            }

          connectionUrl << "logFile=" << "log/log/cci_dbgw.log"
              << "&logOnException=true&logSlowQueries=true";

          m_hCCIConnection = cci_connect_with_url(
              const_cast<char *>(connectionUrl.str().c_str()),
              const_cast<char *>(dbInfoMap["dbuser"].c_str()),
              const_cast<char *>(dbInfoMap["dbpasswd"].c_str()));
          if (m_hCCIConnection < 0)
            {
              CUBRIDException e(m_hCCIConnection, "Failed to connect database.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }
          DBGW_LOG_INFO(m_logger.getLogMessage("connection open.").c_str());

          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    bool DBGWCUBRIDConnection::close()
    {
      clearException();

      try
        {
          if (m_bClosed)
            {
              return true;
            }

          m_bClosed = true;

          if (m_hCCIConnection > 0)
            {
              T_CCI_ERROR cciError;
              int nResult = cci_disconnect(m_hCCIConnection, &cciError);
              if (nResult < 0)
                {
                  CUBRIDException e(nResult, cciError,
                      "Failed to close connection.");
                  DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
                  throw e;
                }

              DBGW_LOG_INFO(m_logger.getLogMessage("connection close.").
                  c_str());
            }

          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    DBGWPreparedStatementSharedPtr DBGWCUBRIDConnection::preparedStatement(
        const DBGWBoundQuerySharedPtr p_query)
    {
      clearException();

      try
        {
          DBGWPreparedStatementSharedPtr pResult(
              new DBGWCUBRIDPreparedStatement(p_query, m_hCCIConnection));
          return pResult;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return DBGWPreparedStatementSharedPtr();
        }
    }

    bool DBGWCUBRIDConnection::setAutocommit(bool bAutocommit)
    {
      clearException();

      try
        {
          int nResult;
          if (bAutocommit)
            {
              nResult = cci_set_autocommit(m_hCCIConnection, CCI_AUTOCOMMIT_TRUE);
            }
          else
            {
              nResult
                = cci_set_autocommit(m_hCCIConnection, CCI_AUTOCOMMIT_FALSE);
            }

          if (nResult < 0)
            {
              CUBRIDException e(nResult, "Failed to set autocommit.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }

          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    bool DBGWCUBRIDConnection::commit()
    {
      clearException();

      try
        {
          T_CCI_ERROR cciError;
          int nResult =
              cci_end_tran(m_hCCIConnection, CCI_TRAN_COMMIT, &cciError);
          if (nResult < 0)
            {
              CUBRIDException e(nResult, cciError, "Failed to commit database.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }
          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    bool DBGWCUBRIDConnection::rollback()
    {
      clearException();

      try
        {
          T_CCI_ERROR cciError;
          int nResult = cci_end_tran(m_hCCIConnection, CCI_TRAN_ROLLBACK,
              &cciError);
          if (nResult < 0)
            {
              CUBRIDException
              e(nResult, cciError, "Failed to rollback database.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }
          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    DBGWCUBRIDPreparedStatement::~DBGWCUBRIDPreparedStatement()
    {
      clearException();

      try
        {
          if (close() < 0)
            {
              throw getLastException();
            }
        }
      catch (DBGWException &e)
        {
          setLastException(e);
        }
    }

    bool DBGWCUBRIDPreparedStatement::close()
    {
      clearException();

      try
        {
          if (m_bClosed)
            {
              return true;
            }

          m_bClosed = true;

          if (m_hCCIRequest > 0)
            {
              clearException();

              int nResult = cci_close_req_handle(m_hCCIRequest);
              if (nResult < 0)
                {
                  CUBRIDException e(nResult, "Failed to close statement.");
                  DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
                  throw e;
                }

              m_hCCIRequest = -1;
              DBGW_LOG_INFO(m_logger.getLogMessage("close statement.").c_str());
            }

          return true;
        }
      catch (DBGWException &e)
        {
          setLastException(e);
          return false;
        }
    }

    DBGWCUBRIDPreparedStatement::DBGWCUBRIDPreparedStatement(
        const DBGWBoundQuerySharedPtr p_query, int hCCIConnection) :
      DBGWPreparedStatement(p_query), m_hCCIConnection(hCCIConnection),
      m_hCCIRequest(-1), m_bClosed(false)
    {
      T_CCI_ERROR cciError;
      m_hCCIRequest = cci_prepare(m_hCCIConnection,
          const_cast<char *>(p_query->getSQL()), 0, &cciError);
      if (m_hCCIRequest < 0)
        {
          CUBRIDException e(m_hCCIRequest, cciError,
              "Failed to prepare statement.");
          DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
          throw e;
        }
      DBGW_LOG_INFO(m_logger.getLogMessage("prepare statement.").c_str());
    }

    void DBGWCUBRIDPreparedStatement::bind()
    {
      int nResult = 0;
      const DBGWValue *pValue = NULL;
      DBGWQueryParameter stParam;
      const DBGWParameter &parameter = getParameter();
      for (int i = 0, size = getQuery()->getBindNum(); i < size; i++)
        {
          stParam = getQuery()->getBindParam(i);
          pValue = parameter.getValue(stParam.name.c_str(), stParam.nIndex);

          switch (pValue->getType())
            {
            case DBGW_VAL_TYPE_INT:
              nResult = doBindInt(i + 1, pValue);
              break;
            case DBGW_VAL_TYPE_STRING:
              nResult = doBindString(i + 1, pValue);
              break;
            case DBGW_VAL_TYPE_LONG:
              nResult = doBindLong(i + 1, pValue);
              break;
            case DBGW_VAL_TYPE_CHAR:
              nResult = doBindChar(i + 1, pValue);
              break;
            default:
              CUBRIDException e(
                  "Failed to bind parameter. invalid parameter type.");
              DBGW_LOG_ERROR(e.what());
              throw e;
            }

          if (nResult < 0)
            {
              CUBRIDException e(nResult, "Failed to bind parameter.");
              DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
              throw e;
            }
        }
    }

    int DBGWCUBRIDPreparedStatement::doBindInt(int nIndex, const DBGWValue *pValue)
    {
      int nValue = 0;
      T_CCI_U_TYPE utype = CCI_U_TYPE_INT;
      if (pValue->getInt(&nValue))
        {
          if (pValue->isNull())
            {
              utype = CCI_U_TYPE_NULL;
            }
          return cci_bind_param(m_hCCIRequest, nIndex, CCI_A_TYPE_INT, &nValue,
              utype, 0);
        }
      else
        {
          return -1;
        }
    }

    int DBGWCUBRIDPreparedStatement::doBindLong(int nIndex, const DBGWValue *pValue)
    {
      int64 lValue = 0;
      T_CCI_U_TYPE utype = CCI_U_TYPE_BIGINT;
      if (pValue->getLong(&lValue))
        {
          if (pValue->isNull())
            {
              utype = CCI_U_TYPE_NULL;
            }
          return cci_bind_param(m_hCCIRequest, nIndex, CCI_A_TYPE_BIGINT,
              &lValue, CCI_U_TYPE_BIGINT, 0);
        }
      else
        {
          return -1;
        }
    }

    int DBGWCUBRIDPreparedStatement::doBindString(int nIndex,
        const DBGWValue *pValue)
    {
      char *szValue;
      T_CCI_U_TYPE utype = CCI_U_TYPE_STRING;
      if (pValue->getCString(&szValue))
        {
          if (pValue->isNull())
            {
              utype = CCI_U_TYPE_NULL;
            }
          return cci_bind_param(m_hCCIRequest, nIndex, CCI_A_TYPE_STR,
              (void *) szValue, CCI_U_TYPE_STRING, 0);
        }
      else
        {
          return -1;
        }
    }

    int DBGWCUBRIDPreparedStatement::doBindChar(int nIndex, const DBGWValue *pValue)
    {
      char szBuffer[2];
      T_CCI_U_TYPE utype = CCI_U_TYPE_CHAR;
      if (pValue->getChar(&szBuffer[0]))
        {
          if (pValue->isNull())
            {
              utype = CCI_U_TYPE_NULL;
            }
          szBuffer[1] = '\0';
          return cci_bind_param(m_hCCIRequest, nIndex, CCI_A_TYPE_STR,
              (void *) szBuffer, CCI_U_TYPE_CHAR, 0);
        }
      else
        {
          return -1;
        }
    }

    DBGWResultSharedPtr DBGWCUBRIDPreparedStatement::doExecute()
    {
      T_CCI_ERROR cciError;

      if (m_hCCIRequest < 0)
        {
          ExecuteBeforePrepareException e;
          DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
          throw e;
        }

      int nResult = cci_execute(m_hCCIRequest, 0, 0, &cciError);
      if (nResult < 0)
        {
          CUBRIDException e(nResult, cciError, "Failed to execute statement.");
          DBGW_LOG_ERROR(m_logger.getLogMessage(e.what()).c_str());
          throw e;
        }

      bool bNeedFetch = false;
      if (getQuery()->getType() == DBGWQueryType::SELECT)
        {
          bNeedFetch = true;
        }

      DBGWResultSharedPtr p(
          new DBGWCUBRIDResult(m_logger, m_hCCIRequest, nResult, bNeedFetch));
      return p;
    }

    DBGWCUBRIDResult::DBGWCUBRIDResult(const DBGWLogger &logger, int hCCIRequest,
        int nAffectedRow, bool bFetchData) :
      DBGWResult(logger, nAffectedRow, bFetchData), m_hCCIRequest(hCCIRequest),
      m_cursorPos(CCI_CURSOR_FIRST)
    {
      /**
       * We don't need to clear error because this api will not make error.
       *
       * clearException();
       *
       * try
       * {
       * 		blur blur blur;
       * }
       * catch (DBGWException &e)
       * {
       * 		setLastException(e);
       * }
       */
    }

    DBGWCUBRIDResult::~DBGWCUBRIDResult()
    {
      /**
       * We don't need to clear error because this api will not make error.
       *
       * clearException();
       *
       * try
       * {
       * 		blur blur blur;
       * }
       * catch (DBGWException &e)
       * {
       * 		setLastException(e);
       * }
       */
    }

    bool DBGWCUBRIDResult::doFirst()
    {
      m_cursorPos = CCI_CURSOR_FIRST;

      return true;
    }

    bool DBGWCUBRIDResult::doNext()
    {
      if (m_cursorPos == CCI_CURSOR_FIRST)
        {
          makeMetaData();
        }

      T_CCI_ERROR cciError;
      int nResult;
      if ((nResult = cci_cursor(m_hCCIRequest, 1, (T_CCI_CURSOR_POS) m_cursorPos,
          &cciError)) == 0)
        {
          m_cursorPos = CCI_CURSOR_CURRENT;
          nResult = cci_fetch(m_hCCIRequest, &cciError);
          if (nResult != CCI_ER_NO_ERROR)
            {
              CUBRIDException e(nResult, cciError, "Failed to fetch data.");
              DBGW_LOG_ERROR(e.what());
              throw e;
            }

          clear();
          int i = 1;
          DBGWValueSharedPtr pValue;
          const MetaDataList *pMetaList = getMetaDataList();
          for (MetaDataList::const_iterator it = pMetaList->begin(); it
              != pMetaList->end(); it++)
            {
              pValue = makeValue(i++, *it);
              put(it->name.c_str(), pValue);
            }
          return true;
        }

      if (nResult == CCI_ER_NO_MORE_DATA)
        {
          return false;
        }
      else
        {
          CUBRIDException e(nResult, cciError, "Failed to move cursor.");
          DBGW_LOG_ERROR(e.what());
          throw e;
        }
    }

    void DBGWCUBRIDResult::doMakeMetadata(MetaDataList &metaList)
    {
      T_CCI_SQLX_CMD cciCmdType;
      int nColNum;
      T_CCI_COL_INFO *pCCIColInfo = cci_get_result_info(m_hCCIRequest,
          &cciCmdType, &nColNum);
      if (pCCIColInfo == NULL)
        {
          CUBRIDException e("Cannot get the cci col info.");
          DBGW_LOG_ERROR(e.what());
          throw e;
        }

      for (int i = 0; i < nColNum; i++)
        {
          Metadata stMetadata;
          stMetadata.name = CCI_GET_RESULT_INFO_NAME(pCCIColInfo, i + 1);
          stMetadata.orgType = CCI_GET_RESULT_INFO_TYPE(pCCIColInfo, i + 1);
          stMetadata.type = convertCCIUTypeToValueType(stMetadata.orgType);
          metaList.push_back(stMetadata);
        }
    }

    DBGWValueSharedPtr DBGWCUBRIDResult::makeValue(int nColNo,
        const Metadata &stMetadata)
    {
      DBGWValueSharedPtr pValue;
      DBGWRawValue rawValue;
      rawValue.szValue = NULL;

      int atype = convertValueTypeToCCIAType(stMetadata.type);
      int utype = stMetadata.orgType;
      int nIndicator = 0;
      int nResult = cci_get_data(m_hCCIRequest, nColNo, atype,
          (void *) &rawValue, &nIndicator);
      if (nResult != CCI_ER_NO_ERROR)
        {
          CUBRIDException e(nResult, "Failed to get data.");
          DBGW_LOG_ERROR(e.what());
          throw e;
        }

      if (stMetadata.type == DBGW_VAL_TYPE_STRING && nIndicator == -1 && (utype
          == CCI_U_TYPE_DOUBLE || utype == CCI_U_TYPE_NUMERIC || utype
          == CCI_U_TYPE_FLOAT))
        {
          pValue = DBGWValueSharedPtr(new DBGWValue("0"));
        }
      else
        {
          char szBuffer[32];
          if (stMetadata.orgType == CCI_U_TYPE_TIMESTAMP || stMetadata.orgType
              == CCI_U_TYPE_DATE || stMetadata.orgType == CCI_U_TYPE_DATETIME)
            {
              convertDateFormat(rawValue, szBuffer, stMetadata.orgType);
            }
          pValue = DBGWValueSharedPtr(new DBGWValue(rawValue, stMetadata.type));
        }

      return pValue;
    }

    void DBGWCUBRIDResult::convertDateFormat(DBGWRawValue &rawValue,
        char *szBuffer, int utype)
    {
      char *p = szBuffer;
      const char *q = rawValue.szValue;

      memcpy(p, q, 10);
      p += 10;
      q += 10;

      if (utype == CCI_U_TYPE_DATE)
        {
          memcpy(p, "T00:00:00+09:00\0", 16);
        }
      else
        {
          *(p++) = 'T';
          memcpy(p, q + 1, 8);
          memcpy(p + 8, "+09:00\0", 7);
        }

      rawValue.szValue = szBuffer;
    }

  }

}
