/************************************************************************************
   Copyright (C) 2020 MariaDB Corporation AB

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not see <http://www.gnu.org/licenses>
   or write to the Free Software Foundation, Inc.,
   51 Franklin St., Fifth Floor, Boston, MA 02110, USA
*************************************************************************************/


#include <deque>

#include "ServerSidePreparedStatement.h"
#include "logger/LoggerFactory.h"
#include "ExceptionFactory.h"
#include "util/ServerPrepareResult.h"
#include "Results.h"
#include "MariaDbParameterMetaData.h"
#include "MariaDbResultSetMetaData.h"

namespace sql
{
  namespace mariadb
{

  const Shared::Logger ServerSidePreparedStatement::logger= LoggerFactory::getLogger(typeid(ServerSidePreparedStatement));
  /**
    * Constructor for creating Server prepared statement.
    *
    * @param connection current connection
    * @param sql Sql String to prepare
    * @param resultSetScrollType one of the following <code>ResultSet</code> constants: <code>
    *     ResultSet.TYPE_FORWARD_ONLY</code>, <code>ResultSet.TYPE_SCROLL_INSENSITIVE</code>, or
    *     <code>ResultSet.TYPE_SCROLL_SENSITIVE</code>
    * @param resultSetConcurrency a concurrency type; one of <code>ResultSet.CONCUR_READ_ONLY</code>
    *     or <code>ResultSet.CONCUR_UPDATABLE</code>
    * @param autoGeneratedKeys a flag indicating whether auto-generated keys should be returned; one
    *     of <code>Statement.RETURN_GENERATED_KEYS</code> or <code>Statement.NO_GENERATED_KEYS</code>
    * @throws SQLException exception
    */
  ServerSidePreparedStatement::ServerSidePreparedStatement(
    MariaDbConnection* connection, const SQLString& _sql,
    int32_t resultSetScrollType,
    int32_t resultSetConcurrency,
    int32_t autoGeneratedKeys,
    Shared::ExceptionFactory& factory)
    : ServerSidePreparedStatement(connection, resultSetScrollType, resultSetConcurrency, autoGeneratedKeys, connection->getProtocol()->isMasterConnection(), factory)
  {
    serverPrepareResult= nullptr;
    sql= _sql;
    prepare(sql);
  }

  ServerSidePreparedStatement::ServerSidePreparedStatement(
    MariaDbConnection* _connection,
    int32_t resultSetScrollType,
    int32_t resultSetConcurrency,
    int32_t autoGeneratedKeys,
    bool _mustExecuteOnMaster,
    Shared::ExceptionFactory& factory)
    : BasePrepareStatement(_connection, resultSetScrollType, resultSetConcurrency, autoGeneratedKeys, factory),
      connection(_connection),
      mustExecuteOnMaster(_mustExecuteOnMaster),
      serverPrepareResult(nullptr)
  {
  }

  /**
    * Clone statement.
    *
    * @param connection connection
    * @return Clone statement.
    * @throws CloneNotSupportedException if any error occur.
    */
  ServerSidePreparedStatement* ServerSidePreparedStatement::clone(MariaDbConnection* connection)
  {
    ServerSidePreparedStatement* clone= new ServerSidePreparedStatement(connection, this->stmt->getResultSetType(), this->stmt->getResultSetConcurrency(),
      this->autoGeneratedKeys, this->mustExecuteOnMaster, this->exceptionFactory);
    clone->metadata= metadata;
    clone->parameterMetaData= this->parameterMetaData;

    try {
      clone->prepare(sql);
    }
    catch (SQLException&) {
      throw SQLException("PreparedStatement could not be cloned"); //CloneNotSupportedException
    }
    return clone;
  }

  void ServerSidePreparedStatement::prepare(const SQLString& sql)
  {
    try {
      serverPrepareResult= connection->getProtocol()->prepare(sql, mustExecuteOnMaster);
      setMetaFromResult();
    }
    catch (SQLException& e) {
      try {
        this->close();
      }
      catch (std::exception&) {
      }
      logger->error("error preparing query", e);
      throw *exceptionFactory->raiseStatementError(connection, stmt.get())->create(e);
    }
  }

  void ServerSidePreparedStatement::setMetaFromResult()
  {
    parameterCount= serverPrepareResult->getParameters().size();
    metadata.reset(new MariaDbResultSetMetaData(serverPrepareResult->getColumns(), connection->getProtocol()->getUrlParser().getOptions(), false));
    // TODO: these transfer of the vector can be optimized for sure
    parameterMetaData.reset(new MariaDbParameterMetaData(serverPrepareResult->getParameters()));
  }

  void ServerSidePreparedStatement::setParameter(int32_t parameterIndex, ParameterHolder* holder)
  {
    // TODO: does it really has to be map? can be, actually
    auto it= currentParameterHolder.find(parameterIndex - 1);
    if (it == currentParameterHolder.end()) {
      Shared::ParameterHolder paramHolder(holder);
      currentParameterHolder.emplace(parameterIndex - 1, paramHolder);
    }
    else {
      it->second.reset(holder);
    }
  }

  void ServerSidePreparedStatement::addBatch()
  {
    validParameters();

    queryParameters.push_back({});

    std::vector<Shared::ParameterHolder>& newSet= queryParameters.back();
    newSet.reserve(currentParameterHolder.size());

    std::for_each(currentParameterHolder.cbegin(), currentParameterHolder.cend(), /*std::back_inserter(queryParameters),*/
      [&newSet](const std::map<int32_t, Shared::ParameterHolder>::value_type& mapEntry) {newSet.push_back(mapEntry.second); });
  }

  void ServerSidePreparedStatement::addBatch(const SQLString& sql)
  {
    throw *exceptionFactory->raiseStatementError(connection, stmt.get())->create("Cannot do addBatch(SQLString) on preparedStatement");
  }

  void ServerSidePreparedStatement::clearBatch()
  {
    queryParameters.clear();
    hasLongData= false;
  }

  ParameterMetaData* ServerSidePreparedStatement::getParameterMetaData()
  {
    return parameterMetaData.get();
  }

  sql::ResultSetMetaData* ServerSidePreparedStatement::getMetaData()
  {
    return metadata.get();
  }

  sql::Ints* ServerSidePreparedStatement::executeBatch()
  {
    stmt->checkClose();
    int32_t queryParameterSize= queryParameters.size();
    if (queryParameterSize ==0) {
      return new sql::Ints();
    }
    executeBatchInternal(queryParameterSize);
    return stmt->getInternalResults()->getCmdInformation()->getUpdateCounts();
  }

  sql::Longs* ServerSidePreparedStatement::executeLargeBatch()
  {
    stmt->checkClose();
    int32_t queryParameterSize= queryParameters.size();
    if (queryParameterSize ==0) {
      return new sql::Longs();
    }
    executeBatchInternal(queryParameterSize);
    return stmt->getInternalResults()->getCmdInformation()->getLargeUpdateCounts();
  }

  void ServerSidePreparedStatement::executeBatchInternal(int32_t queryParameterSize)
  {
    std::lock_guard<std::mutex> localScopeLock(*connection->getProtocol()->getLock());
    stmt->setExecutingFlag();

    try {
      executeQueryPrologue(serverPrepareResult);

      if (stmt->getQueryTimeout() !=0) {
        stmt->setTimerTask(true);
      }
      std::vector<Shared::ParameterHolder> dummy;
      stmt->setInternalResults(
        new Results(
          stmt.get(),
          0,
          true,
          queryParameterSize,
          true,
          stmt->getResultSetType(),
          stmt->getResultSetConcurrency(),
          autoGeneratedKeys,
          connection->getProtocol()->getAutoIncrementIncrement(),
          NULL,
          dummy));


      if ((connection->getProtocol()->getOptions()->useBatchMultiSend || connection->getProtocol()->getOptions()->useBulkStmts)
       && (connection->getProtocol()->executeBatchServer(
                                                        mustExecuteOnMaster,
                                                        serverPrepareResult,
                                                        stmt->getInternalResults(),
                                                        sql,
                                                        queryParameters,
                                                        hasLongData)))
      {
        if (!metadata) {
          setMetaFromResult();
        }
        stmt->getInternalResults()->commandEnd();
        return;
      }

      SQLException exception("");
      bool exceptionSet= false;
      if (stmt->getQueryTimeout() > 0)
      {
        for (int32_t counter= 0; counter < queryParameterSize; counter++)
        {
          // TODO: verify if paramsets are guaranteed to exist at this point for all queryParameterSize
          std::vector<Shared::ParameterHolder>& parameterHolder= queryParameters[counter];
          try {
            connection->getProtocol()->stopIfInterrupted();
            serverPrepareResult->resetParameterTypeHeader();
            connection->getProtocol()->executePreparedQuery(mustExecuteOnMaster, serverPrepareResult, stmt->getInternalResults(), parameterHolder);
          }
          catch (SQLException& queryException)
          {
            if (connection->getProtocol()->getOptions()->continueBatchOnError
              && connection->getProtocol()->isConnected()
              &&!connection->getProtocol()->isInterrupted())
            {
              if (exceptionSet) {
                exception= queryException;
                exceptionSet= true;
              }
            }
            else {
              throw queryException;
            }
          }
        }
      }
      else {
        for (int32_t counter= 0; counter < queryParameterSize; counter++) {
          std::vector<Shared::ParameterHolder>& parameterHolder= queryParameters[counter];
          try {
            serverPrepareResult->resetParameterTypeHeader();
            connection->getProtocol()->executePreparedQuery(
              mustExecuteOnMaster, serverPrepareResult, stmt->getInternalResults(), parameterHolder);
          }
          catch (SQLException& queryException) {
            if (connection->getProtocol()->getOptions()->continueBatchOnError) {
              if (!exceptionSet) {
                exception= queryException;
              }
            }
            else {
              throw queryException;
            }
          }
        }
      }
      if (exceptionSet) {
        throw exception;
      }

      stmt->getInternalResults()->commandEnd();
    }
    catch (SQLException& initialSqlEx) {
      throw stmt->executeBatchExceptionEpilogue(initialSqlEx, queryParameterSize);
    }
    stmt->executeBatchEpilogue();
  }

  // must have "lock" locked before invoking
  void ServerSidePreparedStatement::executeQueryPrologue(ServerPrepareResult* serverPrepareResult)
  {
    stmt->setExecutingFlag();

    stmt->checkClose();

    connection->getProtocol()->prologProxy(
      serverPrepareResult, stmt->getMaxRows(), connection->getProtocol()->getProxy()/*!= NULL*/, connection, this->stmt.get());
  }

  ResultSet* ServerSidePreparedStatement::executeQuery()
  {
    if (execute()) {
      return stmt->getInternalResults()->releaseResultSet();
    }
    return SelectResultSet::createEmptyResultSet();
  }


  void ServerSidePreparedStatement::clearParameters()
  {
    currentParameterHolder.clear();
  }


  void ServerSidePreparedStatement::validParameters()
  {
    for (int32_t i= 0; i < parameterCount; i++)
    {
      if (currentParameterHolder.find(i) == currentParameterHolder.end())
      {
        logger->error("Parameter at position " + std::to_string(i + 1) + " is not set" );
        throw *exceptionFactory->raiseStatementError(connection, stmt.get())->create("Parameter at position "+ std::to_string(i+1) + " is not set", "07004");
      }
    }
  }


  bool ServerSidePreparedStatement::executeInternal(int32_t fetchSize)
  {
    validParameters();

    std::lock_guard<std::mutex> localScopeLock(*connection->getProtocol()->getLock());
    try {
      executeQueryPrologue(serverPrepareResult);
      if (stmt->getQueryTimeout() !=0) {
        stmt->setTimerTask(false);
      }

      std::vector<Shared::ParameterHolder> parameterHolders;
      std::for_each(currentParameterHolder.cbegin(), currentParameterHolder.cend(), /*std::back_inserter(queryParameters),*/
        [&parameterHolders](const std::map<int32_t, Shared::ParameterHolder>::value_type& mapEntry) {parameterHolders.push_back(mapEntry.second); });

      stmt->setInternalResults(
        new Results(
          this->stmt.get(),
          fetchSize,
          false,
          1,
          true,
          stmt->getResultSetType(),
          stmt->getResultSetConcurrency(),
          autoGeneratedKeys,
          connection->getProtocol()->getAutoIncrementIncrement(),
          sql,
          parameterHolders));

      serverPrepareResult->resetParameterTypeHeader();
      connection->getProtocol()->executePreparedQuery(
        mustExecuteOnMaster, serverPrepareResult, stmt->getInternalResults(), parameterHolders);

      stmt->getInternalResults()->commandEnd();
      return stmt->getInternalResults()->getResultSet() != nullptr;

    }
    catch (SQLException& exception) {
      throw stmt->executeExceptionEpilogue(exception);
    }
    stmt->executeEpilogue();
  }

  void ServerSidePreparedStatement::close()
  {
    std::lock_guard<std::mutex> localScopeLock(*connection->getProtocol()->getLock());

    stmt->markClosed();
    if (stmt->getInternalResults()) {
      if (stmt->getInternalResults()->getFetchSize()!=0) {
        stmt->skipMoreResults();
      }
      stmt->getInternalResults()->close();
    }

    if (serverPrepareResult != nullptr && connection->getProtocol()) {
      try {
        serverPrepareResult->getUnProxiedProtocol()->releasePrepareStatement(serverPrepareResult);
      }
      catch (SQLException&) {
      }
    }
    connection->getProtocol()->reset();
    if (connection
      ||connection->pooledConnection
      ||connection->pooledConnection->noStmtEventListeners()) {
      return;
    }
    connection->pooledConnection->fireStatementClosed(this);
    connection= NULL;
  }

  int32_t ServerSidePreparedStatement::getParameterCount() const
  {
    return parameterCount;
  }

  /**
    * Return sql String value.
    *
    * @return String representation
    */
  SQLString ServerSidePreparedStatement::toString()
  {
    SQLString sb("sql : '"+serverPrepareResult->getSql()+"'");
    if (parameterCount > 0) {
      sb.append(", parameters : [");
      for (int32_t i= 0; i < parameterCount; i++)
      {
        const auto cit= currentParameterHolder.find(i);
        if (cit == currentParameterHolder.cend() || cit->second == NULL) {
          sb.append("NULL");
        }
        else {
          sb.append(cit->second->toString());
        }
        if (i !=parameterCount -1) {
          sb.append(",");
        }
      }
      sb.append("]");
    }
    return sb;
  }

  /**
    * Permit to retrieve current connection thread id, or -1 if unknown.
    *
    * @return current connection thread id.
    */
  int64_t ServerSidePreparedStatement::getServerThreadId()
  {
    return serverPrepareResult->getUnProxiedProtocol()->getServerThreadId();
  }
}
}
