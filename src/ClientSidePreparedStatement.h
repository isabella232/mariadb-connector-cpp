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


#ifndef _CLIENTSIDEPREPAREDSTATEMENT_H_
#define _CLIENTSIDEPREPAREDSTATEMENT_H_

#include "Consts.h"

#include "BasePrepareStatement.h"
#include "MariaDbStatement.h"

#include "parameters/ParameterHolder.h"

namespace sql
{
namespace mariadb
{

class ClientSidePreparedStatement : public BasePrepareStatement {

  static const Shared::Logger logger ; /*LoggerFactory.getLogger(typeid(ClientSidePreparedStatement))*/
  const std::vector<ParameterHolder>parameterList;
  Shared::ClientPrepareResult prepareResult;
  SQLString sqlQuery;
  std::vector<ParameterHolder> parameters;
  Shared::ResultSetMetaData resultSetMetaData; /*NULL*/
  Shared::ParameterMetaData parameterMetaData ; /*NULL*/

public:
  ClientSidePreparedStatement(
    MariaDbConnection* connection, const SQLString& sql,
    int32_t resultSetScrollType,
    int32_t resultSetConcurrency,
    int32_t autoGeneratedKeys);

  ClientSidePreparedStatement* clone(MariaDbConnection connection);
  bool execute();
  ResultSet* executeQuery();
  int32_t executeUpdate();

  bool execute(const SQLString& sql) { return stmt->execute(sql); }
  bool execute(const SQLString& sql, int32_t autoGeneratedKeys) { return stmt->execute(sql, autoGeneratedKeys); }
  bool execute(const SQLString& sql, int32_t* columnIndexes) { return stmt->execute(sql, columnIndexes); }
  bool execute(const SQLString& sql, const SQLString* columnNames) { return stmt->execute(sql, columnNames); }
  ResultSet* executeQuery(const SQLString& sql) { return stmt->executeQuery(sql); }
  int32_t executeUpdate(const SQLString& sql) { return stmt->executeUpdate(sql); }
  int32_t executeUpdate(const SQLString& sql, int32_t autoGeneratedKeys) { return stmt->executeUpdate(sql, autoGeneratedKeys); }
  int32_t executeUpdate(const SQLString& sql, int32_t* columnIndexes) { return stmt->executeUpdate(sql, columnIndexes); }
  int32_t executeUpdate(const SQLString& sql, const SQLString* columnNames) { return stmt->executeUpdate(sql, columnNames); }
  int64_t executeLargeUpdate(const SQLString& sql) { return stmt->executeLargeUpdate(sql); }
  int64_t executeLargeUpdate(const SQLString& sql, int32_t autoGeneratedKeys) { return stmt->executeLargeUpdate(sql, autoGeneratedKeys); }
  int64_t executeLargeUpdate(const SQLString& sql, int32_t* columnIndexes) { return stmt->executeLargeUpdate(sql, columnIndexes); }
  int64_t executeLargeUpdate(const SQLString& sql, SQLString* columnNames) { return stmt->executeLargeUpdate(sql, columnNames); }
  int32_t getMaxFieldSize() { return stmt->getMaxFieldSize(); }
  void setMaxFieldSize(int32_t max) { stmt->setMaxFieldSize(max); }
  int32_t getMaxRows() { return stmt->getMaxRows(); }
  void setMaxRows(int32_t max) { stmt->setMaxRows(max); }
  int64_t getLargeMaxRows() { return stmt->getLargeMaxRows(); }
  void setLargeMaxRows(int64_t max) { stmt->setLargeMaxRows(max); }
  void setEscapeProcessing(bool enable) { stmt->setEscapeProcessing(enable); }
  int32_t getQueryTimeout() { return stmt->getQueryTimeout(); }
  void setQueryTimeout(int32_t seconds) { stmt->setQueryTimeout(seconds); }
  void cancel() { stmt->cancel(); }
  SQLWarning* getWarnings() { return stmt->getWarnings(); }
  void clearWarnings() { stmt->clearWarnings(); }
  void setCursorName(const SQLString& name) { stmt->setCursorName(name);  }
  Connection* getConnection() { return stmt->getConnection(); }
  ResultSet* getGeneratedKeys() { return stmt->getGeneratedKeys(); }
  int32_t getResultSetHoldability() { return stmt->getResultSetHoldability(); }
  bool isClosed() { return stmt->isClosed(); }
  bool isPoolable() { return stmt->isPoolable(); }
  void setPoolable(bool poolable) { stmt->setPoolable(poolable); }
  ResultSet* getResultSet() { return stmt->getResultSet(); }
  int32_t getUpdateCount() { return stmt->getUpdateCount(); }
  int64_t getLargeUpdateCount() { return stmt->getLargeUpdateCount(); }
  bool getMoreResults() { return stmt->getMoreResults(); }
  bool getMoreResults(int32_t current) { return stmt->getMoreResults(current); }
  int32_t getFetchDirection() { return stmt->getFetchDirection(); }
  void setFetchDirection(int32_t direction) { stmt->setFetchDirection(direction); }
  int32_t getFetchSize() { return stmt->getFetchSize(); }
  void setFetchSize(int32_t rows) { stmt->setFetchSize(rows); }
  int32_t getResultSetConcurrency() { return stmt->getResultSetConcurrency(); }
  int32_t getResultSetType() { return stmt->getResultSetType(); }
  void closeOnCompletion() { stmt->closeOnCompletion(); }
  bool isCloseOnCompletion() { return stmt->isCloseOnCompletion(); }

protected:
  bool executeInternal(int32_t fetchSize);
public:
  void addBatch();
  void addBatch(const SQLString& sql);
  void clearBatch();
  sql::Ints* executeBatch();
  sql::Ints* getServerUpdateCounts();
  sql::Longs* executeLargeBatch();

private:
  void executeInternalBatch(int32_t size);
public:
  sql::ResultSetMetaData* getMetaData();
  void setParameter(int32_t parameterIndex,const ParameterHolder& holder);
  ParameterMetaData* getParameterMetaData();
private:
  void loadParametersData();
public:  void clearParameters();
  void close();
protected:
  int32_t getParameterCount();
public:
  SQLString toString();
protected:
  ClientPrepareResult getPrepareResult();
  };
}
}
#endif