/*
 * This file is part of the Jose Project
 * see http://jose-chess.sourceforge.net/
 * (c) 2002-2006 Peter Sch�fer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */


package de.jose.pool;

import java.sql.*;
import java.util.Map;

/**
 * PooledConnection
 *
 * @author Peter Sch�fer
 */

public class PooledConnection
{
    /**  the underlying connection   */
    protected Connection jdbcConnection;
    /** pool key    */
    protected Object poolKey;
    /** pool    */
    protected ConnectionPool pool;

    public PooledConnection(ConnectionPool pool, Object key, Connection theConnection)
    {
        jdbcConnection = theConnection;
    }

    public void release()
    {
        try {
            if (poolKey!=null)
                pool.releaseConnection(this,poolKey);
         } finally {
            poolKey = null;
        }
    }

    public void close() throws SQLException
    {
        jdbcConnection.close();
        try {
            if (poolKey!=null)
                pool.finishConnection(this,poolKey);
         } finally {
            poolKey = null;
        }
    }


    // ----------------------------------------
    //  Delegate to underlying connection
    // ----------------------------------------

    public void clearWarnings() throws SQLException {
        jdbcConnection.clearWarnings();
    }

    public void commit() throws SQLException {
        jdbcConnection.commit();
    }

    public Statement createStatement() throws SQLException {
        return jdbcConnection.createStatement();
    }

    public Statement createStatement(int resultSetType, int resultSetConcurrency)
	throws SQLException {
        return jdbcConnection.createStatement(resultSetType, resultSetConcurrency);
    }

    public Statement createStatement(int resultSetType, int resultSetConcurrency,
			      int resultSetHoldability) throws SQLException {
        return jdbcConnection.createStatement(resultSetType, resultSetConcurrency, resultSetHoldability);
    }

    public boolean getAutoCommit() throws SQLException {
        return jdbcConnection.getAutoCommit();
    }

    public String getCatalog() throws SQLException {
        return jdbcConnection.getCatalog();
    }

    public int getHoldability() throws SQLException {
        return jdbcConnection.getHoldability();
    }

    public DatabaseMetaData getMetaData() throws SQLException {
        return jdbcConnection.getMetaData();
    }

    public int getTransactionIsolation() throws SQLException {
        return jdbcConnection.getTransactionIsolation();
    }

    public Map getTypeMap() throws SQLException {
        return jdbcConnection.getTypeMap();
    }

    public SQLWarning getWarnings() throws SQLException {
        return jdbcConnection.getWarnings();
    }

    public boolean isClosed() throws SQLException {
        return jdbcConnection.isClosed();
    }

    public boolean isReadOnly() throws SQLException {
        return jdbcConnection.isReadOnly();
    }

    public String nativeSQL(String sql) throws SQLException {
        return jdbcConnection.nativeSQL(sql);
    }

    public CallableStatement prepareCall(String sql) throws SQLException {
        return jdbcConnection.prepareCall(sql);
    }

    public CallableStatement prepareCall(String sql, int resultSetType,
				  int resultSetConcurrency) throws SQLException {
        return jdbcConnection.prepareCall(sql, resultSetType, resultSetConcurrency);
    }

    public CallableStatement prepareCall(String sql, int resultSetType,
				  int resultSetConcurrency,
				  int resultSetHoldability) throws SQLException {
        return jdbcConnection.prepareCall(sql, resultSetType, resultSetConcurrency, resultSetHoldability);
    }

    public PreparedStatement prepareStatement(String sql)
	throws SQLException {
        return jdbcConnection.prepareStatement(sql);
    }

    public PreparedStatement prepareStatement(String sql, int autoGeneratedKeys)
	throws SQLException {
        return jdbcConnection.prepareStatement(sql, autoGeneratedKeys);
    }

    public PreparedStatement prepareStatement(String sql, int columnIndexes[])
	throws SQLException {
        return jdbcConnection.prepareStatement(sql, columnIndexes);
    }

    public PreparedStatement prepareStatement(String sql, String columnNames[])
	throws SQLException {
        return jdbcConnection.prepareStatement(sql, columnNames);
    }

    public PreparedStatement prepareStatement(String sql, int resultSetType,
				       int resultSetConcurrency)
	throws SQLException {
        return jdbcConnection.prepareStatement(sql, resultSetType, resultSetConcurrency);
    }

    public PreparedStatement prepareStatement(String sql, int resultSetType,
				       int resultSetConcurrency, int resultSetHoldability)
	throws SQLException {
        return jdbcConnection.prepareStatement(sql, resultSetType, resultSetConcurrency, resultSetHoldability);
    }

    public void releaseSavepoint(Savepoint savepoint) throws SQLException {
        jdbcConnection.releaseSavepoint(savepoint);
    }

    public void rollback() throws SQLException {
        jdbcConnection.rollback();
    }

    public void rollback(Savepoint savepoint) throws SQLException {
        jdbcConnection.rollback(savepoint);
    }

    public void setAutoCommit(boolean autoCommit) throws SQLException {
        jdbcConnection.setAutoCommit(autoCommit);
    }

    public void setCatalog(String catalog) throws SQLException {
        jdbcConnection.setCatalog(catalog);
    }

    public void setHoldability(int holdability) throws SQLException {
        jdbcConnection.setHoldability(holdability);
    }

    public void setReadOnly(boolean readOnly) throws SQLException {
        jdbcConnection.setReadOnly(readOnly);
    }

    public Savepoint setSavepoint() throws SQLException {
        return jdbcConnection.setSavepoint();
    }

    public Savepoint setSavepoint(String name) throws SQLException {
        return jdbcConnection.setSavepoint(name);
    }

    public void setTransactionIsolation(int level) throws SQLException {
        jdbcConnection.setTransactionIsolation(level);
    }

    public void setTypeMap(Map map) throws SQLException {
        jdbcConnection.setTypeMap(map);
    }
}