/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2016-2023 Maxar (http://www.maxar.com/)
 */
package hoot.services.utils;


import static hoot.services.HootProperties.CHANGESETS_FOLDER;
import static hoot.services.models.db.QCommandStatus.commandStatus;
import static hoot.services.models.db.QFolderMapMappings.folderMapMappings;
import static hoot.services.models.db.QFolders.folders;
import static hoot.services.models.db.QJobStatus.jobStatus;
import static hoot.services.models.db.QMaps.maps;
import static hoot.services.models.db.QReviewBookmarks.reviewBookmarks;
import static hoot.services.models.db.QTranslations.translations;
import static hoot.services.models.db.QTranslationFolders.translationFolders;
import static hoot.services.models.db.QUsers.users;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FilenameFilter;
import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.sql.Statement;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.inject.Provider;
import javax.sql.DataSource;
import javax.ws.rs.BadRequestException;
import javax.ws.rs.NotFoundException;
import javax.ws.rs.WebApplicationException;

import org.apache.commons.lang3.StringUtils;
import org.json.simple.JSONObject;
import org.json.simple.parser.JSONParser;
import org.json.simple.parser.ParseException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.context.ApplicationContext;
import org.springframework.transaction.annotation.Transactional;

import com.querydsl.core.Tuple;
import com.querydsl.core.types.dsl.BooleanExpression;
import com.querydsl.core.types.dsl.Expressions;
import com.querydsl.core.types.dsl.NumberPath;
import com.querydsl.core.types.dsl.StringPath;
import com.querydsl.sql.Configuration;
import com.querydsl.sql.PostgreSQLTemplates;
import com.querydsl.sql.RelationalPathBase;
import com.querydsl.sql.SQLQuery;
import com.querydsl.sql.SQLQueryFactory;
import com.querydsl.sql.SQLTemplates;
import com.querydsl.sql.dml.SQLUpdateClause;
import com.querydsl.sql.namemapping.PreConfiguredNameMapping;
import com.querydsl.sql.spring.SpringConnectionProvider;
import com.querydsl.sql.spring.SpringExceptionTranslator;
import com.querydsl.sql.types.EnumAsObjectType;

import hoot.services.ApplicationContextUtils;
import hoot.services.command.CommandResult;
import hoot.services.controllers.osm.user.UserResource;
import hoot.services.models.db.TranslationFolder;
import hoot.services.models.db.Folders;
import hoot.services.models.db.JobStatus;
import hoot.services.models.db.Maps;
import hoot.services.models.db.QFolders;
import hoot.services.models.db.QUsers;
import hoot.services.models.db.Translations;
import hoot.services.models.db.Users;


/**
 * General Hoot services database utilities
 */
@Transactional
public class DbUtils {
    private static final Logger logger = LoggerFactory.getLogger(DbUtils.class);

    private static final SQLTemplates templates = PostgreSQLTemplates.builder().quote().build();


    private DbUtils() {}

    /**
     * The types of operations that can be performed on an OSM element from a
     * changeset upload request
     */
    public enum EntityChangeType {
        CREATE, MODIFY, DELETE
    }

    /**
     * The types of operations that can be performed when writing OSM data to
     * the services database
     */
    public enum RecordBatchType {
        INSERT, UPDATE, DELETE
    }

    public enum nwr_enum {
        node, way, relation
    }

    public static Configuration getConfiguration() {
        return getConfiguration("");
    }

    public static Configuration getConfiguration(long mapId) {
        return getConfiguration(String.valueOf(mapId));
    }

    private static Configuration getConfiguration(String mapId) {
        Configuration configuration = new Configuration(templates);
        configuration.register("current_relation_members", "member_type", new EnumAsObjectType<>(nwr_enum.class));
        configuration.setExceptionTranslator(new SpringExceptionTranslator());

        if ((mapId != null) && (!mapId.isEmpty())) {
            overrideTable(mapId, configuration);
        }

        return configuration;
    }

    private static void overrideTable(String mapId, Configuration config) {
        if (config != null) {

            PreConfiguredNameMapping tablemapping = new PreConfiguredNameMapping();
            tablemapping.registerTableOverride("current_relation_members", "current_relation_members_" + mapId);
            tablemapping.registerTableOverride("current_relations", "current_relations_" + mapId);
            tablemapping.registerTableOverride("current_way_nodes", "current_way_nodes_" + mapId);
            tablemapping.registerTableOverride("current_ways", "current_ways_" + mapId);
            tablemapping.registerTableOverride("current_nodes", "current_nodes_" + mapId);
            tablemapping.registerTableOverride("changesets", "changesets_" + mapId);
            config.setDynamicNameMapping(tablemapping);
        }
    }

    public static Connection getConnection() throws SQLException {
        ApplicationContext applicationContext = ApplicationContextUtils.getApplicationContext();
        DataSource datasource = applicationContext.getBean("dataSource", DataSource.class);
        return datasource.getConnection();
    }

    public static SQLQueryFactory createQuery(String mapId) {
        ApplicationContext applicationContext = ApplicationContextUtils.getApplicationContext();
        DataSource datasource = applicationContext.getBean("dataSource", DataSource.class);
        Provider<Connection> provider = new SpringConnectionProvider(datasource);
        return new SQLQueryFactory(getConfiguration(mapId), provider);
    }

    public static SQLQueryFactory createQuery() {
        return createQuery(null);
    }

    public static SQLQueryFactory createQuery(long mapId) {
        return createQuery(String.valueOf(mapId));
    }

    /**
     * Gets the map id from map name
     *
     * @param mapName map name
     * @param userId user id
     * @return map ID
     */
    public static Long getMapIdByName(String mapName, Long userId) {
        //FIXME: the user check here is in case there are duplicate map names for different users
        //but it does not account for maps in public folders
        return createQuery().select(maps.id).from(maps).where(maps.displayName.eq(mapName).and(maps.userId.eq(userId))).fetchOne();
    }

    /**
     * Gets the map id using the job id
     *
     * @param jobId jobs id
     * @return map ID
     */
    public static Long getMapIdByJobId(String jobId) {
        return createQuery()
                .select(jobStatus.resourceId)
                .from(jobStatus)
                .where(jobStatus.jobId.eq(jobId))
                .fetchOne();
    }

    /**
     * Gets the job id using the map id
     *
     * @param mapId map id which was created from a job
     * @return job id
     */
    public static String getJobIdByMapId(Long mapId) {
        return createQuery()
            .select(jobStatus.jobId)
            .from(jobStatus)
            .where(jobStatus.resourceId.eq(mapId))
            .fetchOne();
    }

    /**
     * Deletes the specified mapId
     * @param mapId
     */
    public static void deleteMap(Long mapId) {
        createQuery().delete(maps)
                .where(maps.id.eq(mapId))
                .execute();

        deleteFolderMapping(mapId);
    }

    /**
     * Creates a new folder under the parent directory
     * if not already present and returns it's id
     *
     * @param folderName folder name
     * @param parentId parent directory id that the folder will get linked to
     */
    public static Long createFolder(String folderName, Long parentId, Long userId, Boolean isPublic) {
        Long folderId = getFolderByNameForUser(folderName, parentId, userId);

        if (folderId == null) {
            folderId = createQuery()
                    .select(Expressions.numberTemplate(Long.class, "nextval('folders_id_seq')"))
                    .from()
                    .fetchOne();

            Timestamp now = new Timestamp(System.currentTimeMillis());


            createQuery()
                    .insert(folders).columns(folders.id, folders.createdAt, folders.displayName, folders.publicCol, folders.userId, folders.parentId)
                    .values(folderId, now, folderName, isPublic, userId, parentId).execute();
        }

        return folderId;
    }

    public static Long getFolderByNameForUser(String name, Long parentId, Long userId) {
        SQLQuery<Long> sql = createQuery()
                .select(folders.id)
                .from(folders)
                .where(folders.displayName.eq(name).and(folders.parentId.eq(parentId).and(folders.userId.eq(userId))));
        return sql.fetchFirst();
    }

    public static Set<Long> getFolderIdsForUser(Users user) {
        Long userId = null;
        if (user != null) userId = user.getId();
        return getFolderIdsForUser(userId);
    }

    public static Set<Long> getFolderIdsForUser(Long userId) {
        List<Folders> folders = getFoldersForUser(userId);
        Set<Long> out = new HashSet<Long>(folders.size());
        for (Folders f : folders) {
            out.add(f.getId());
        }
        return out;
    }

    public static List<Folders> getFoldersForUser(Users user) {
        Long userId = null;
        if (user != null) userId = user.getId();
        return getFoldersForUser(userId);
    }

    public static List<Folders> getFoldersForUser(Long userId) {
        SQLQuery<Folders> sql = createQuery()
            .select(folders)
            .from(folders)
            .where(folders.id.ne(0L));
        if (userId != null && !UserResource.adminUserCheck(getUser(userId))) {
            sql.where(
                    folders.userId.eq(userId).or(folders.publicCol.isTrue())
            );
        }
        List<Folders> folderRecordSet = sql.orderBy(folders.displayName.asc()).fetch();

        return folderRecordSet;
    }

    public static List<Long> getChildrenFolders(Long folderId) {
        List<Long> childrenFolders = createQuery()
                .select(folders.id)
                .from(folders)
                .where(folders.parentId.eq(folderId))
                .fetch();

        return childrenFolders;
    }

    public static List<Tuple> getMapsForUser(Users user) {
        // return empty list if user is null
        if(user == null) {
            return Collections.emptyList();
        }

        SQLQuery<Tuple> q = createQuery()
                .select(maps, folders.id, folders.publicCol)
                .from(maps)
                .leftJoin(folderMapMappings).on(folderMapMappings.mapId.eq(maps.id))
                .leftJoin(folders).on(folders.id.eq(folderMapMappings.folderId))
                .orderBy(maps.displayName.asc());

        // if user is not admin enforce visiblity rules
        // admins can see everything
        if (!UserResource.adminUserCheck(user)) {
            BooleanExpression isVisible = maps.userId.eq(user.getId()) // Owned by the current user
                .or(folderMapMappings.id.isNull().or(folderMapMappings.folderId.eq(0L)) // or not in a folder
                .or(folders.publicCol.isTrue()));// or in a public folder

            q.where(isVisible);
        }

        return q.fetch();
    }

    /*
     * --Deletes folders that are empty (no child datasets or folders)
    DELETE
    FROM folders f
    WHERE
    NOT EXISTS
        (
        SELECT  NULL
        FROM    folder_map_mappings fmm
        WHERE   f.id = fmm.folder_id
        )
    AND
    NOT EXISTS
        (
        SELECT  NULL
        FROM    folders f2
        WHERE   f.id = f2.parent_id
        );
     */
    public static void deleteEmptyFolders() {
        QFolders folders2 = new QFolders("folders2");
        BooleanExpression isEmpty = new SQLQuery<>().from(folderMapMappings).where(folderMapMappings.folderId.eq(folders.id)).notExists()
                .and(new SQLQuery<>().from(folders2).where(folders.id.eq(folders2.parentId)).notExists());

        SQLQuery<Folders> hasEmpty = createQuery()
            .select(folders)
            .from(folders)
            .where(isEmpty);

        //keep trying to delete newly empty folders until no more are detected
        while (hasEmpty.fetchCount() > 0) {
            createQuery()
                .delete(folders)
                .where(isEmpty)
                .execute();
        }
    }

    /**
     * Sets the parent directory for the specified folder
     *
     * @param folderId folder id whos parent we are setting
     * @param parent parent folder that the folder will get linked to
     */
    public static void setFolderParent(Long folderId, Folders parent) {
        SQLUpdateClause query = createQuery()
                .update(folders)
                .where(folders.id.eq(folderId))
                .set(folders.parentId, parent.getId());

        // dont want a private folder moved to root to become public
        // otherwise inherit the visibility of the new parent folder
        if(parent.getId() != 0) {
            query.set(folders.publicCol, parent.getPublicCol());
        }

        query.execute();
    }

    public static String getDisplayNameById(long mapId) {
        return createQuery().select(maps.displayName).from(maps).where(maps.id.eq(mapId)).fetchOne();
    }

    public static Long getMapIdFromRef(String mapRef, Long userId) {
        Long mapId;
        try {
            mapId = Long.parseLong(mapRef);
        }
        catch (NumberFormatException ignored) {
            mapId = getMapIdByName(mapRef, userId);
        }
        if(mapId == null) {
            throw new IllegalArgumentException(mapRef + " doesn't have a corresponding map ID associated with it!");
        }
        return mapId;
    }

    /**
     *  Delete map related tables by map ID
     *
     * @param mapId map ID
     */
    public static void deleteMapRelatedTablesByMapId(long mapId) {
        List<String> tables = new ArrayList<>();

        tables.add("current_way_nodes_" + mapId);
        tables.add("current_relation_members_" + mapId);
        tables.add("current_nodes_" + mapId);
        tables.add("current_ways_" + mapId);
        tables.add("current_relations_" + mapId);
        tables.add("changesets_" + mapId);

        try {
            deleteTables(tables);
        }
        catch (SQLException e) {
            throw new RuntimeException("Error deleting map related tables by map id.  mapId = " + mapId, e);
        }

        List<String> sequences = new ArrayList<>();

        sequences.add("current_nodes_" + mapId + "_id_seq");
        sequences.add("current_ways_" + mapId + "_id_seq");
        sequences.add("current_relations_" + mapId + "_id_seq");
        sequences.add("changesets_" + mapId + "_id_seq");

        try {
            deleteSequences(sequences);
        }
        catch (SQLException e) {
            throw new RuntimeException("Error deleting map related sequences by map id.  mapId = " + mapId, e);
        }
    }

    public static long getTestUserId() {
        // there is only ever one test user
        return createQuery().select(QUsers.users.id).from(QUsers.users).fetchFirst();
    }

    public static long updateMapsTableTags(Map<String, String> tags, long mapId) {
        return createQuery(mapId).update(maps)
                .where(maps.id.eq(mapId))
                .set(Collections.singletonList(maps.tags),
                        Collections.singletonList(Expressions.stringTemplate("COALESCE(tags, '') || {0}::hstore", tags)))
                .execute();
    }

    public static boolean grailEligible(long inputId) {
        Map<String, String> tags = getMapsTableTags(inputId);
        String grailReference = tags.get("grailReference");

        return grailReference != null && grailReference.equals("true");
    }

    public static String getConflationType(long inputId) {
        String conflationType = null;
        Map<String, String> tags = getMapsTableTags(inputId);
        String sourceInfo = tags.get("params");

        if(sourceInfo != null) {
            JSONParser parser = new JSONParser();
            JSONObject expectedObj;

            try {
                sourceInfo = sourceInfo.replace("\\", "").replace("\"{", "{").replace("}\"", "}"); // have to unescape
                expectedObj = (JSONObject) parser.parse(sourceInfo);
            }
            catch (ParseException e) {
                throw new RuntimeException("Error parsing params json string.  mapId = " + inputId, e);
            }

            conflationType = expectedObj.get("CONFLATION_TYPE").toString();
        }

        return conflationType;
    }

    /**
     * Check to see if the map contains the tag bounds or bbox and return it, else null
     * @param mapId
     * @return bounds string for the map, null if doesnt exist
     */
    public static String getMapBounds(long mapId) {
        Map<String, String> tags = getMapsTableTags(mapId);
        return tags.get("bounds") != null ? tags.get("bounds") : tags.get("bbox");
    }

    /**
     * Inserts a mapid to the folder mapping table if it doesn't exist
     * Updates mapid's parent if it does exist
     *
     * @param mapId map id whos parent we are setting
     * @param folderId parent directory id that map id will get linked to
     */
    public static void updateFolderMapping(Long mapId, Long folderId) {
        long recordCount = createQuery()
            .from(folderMapMappings)
            .where(folderMapMappings.mapId.eq(mapId))
            .fetchCount();

        if (recordCount == 0) {
            createQuery()
                .insert(folderMapMappings)
                .columns(folderMapMappings.mapId, folderMapMappings.folderId)
                .values(mapId, folderId)
                .execute();
        } else {
            createQuery()
                .update(folderMapMappings)
                .where(folderMapMappings.mapId.eq(mapId))
                .set(folderMapMappings.folderId, folderId)
                .execute();
        }
    }

    /**
     * Deletes the folder_map_mapping record with the specified mapId
     * @param mapId
     */
    public static void deleteFolderMapping(Long mapId) {
        createQuery().delete(folderMapMappings)
            .where(folderMapMappings.mapId.eq(mapId))
            .execute();
    }

    public static Map<String, String> getMapsTableTags(long mapId) {
        Map<String, String> tags = new HashMap<>();

        List<Object> results = createQuery(mapId).select(maps.tags).from(maps).where(maps.id.eq(mapId)).fetch();

        if (!results.isEmpty()) {
            Object oTag = results.get(0);
            tags = PostgresUtils.postgresObjToHStore(oTag);
        }

        return tags;
    }

    public static void deleteTables(List<String> tables) throws SQLException {
        try (Connection conn = getConnection()) {
            for (String table : tables) {
                // DDL Statement. No support in QueryDSL anymore. Have to do it the old-fashioned way.
                String sql = "DROP TABLE IF EXISTS \"" + table + "\"";
                try (PreparedStatement stmt = conn.prepareStatement(sql)) {
                    stmt.execute();
                    stmt.close();
                }
            }
            if(!conn.getAutoCommit()) {
                conn.commit();
            }
        }
    }

    public static void deleteSequences(List<String> sequences) throws SQLException {
        try (Connection conn = getConnection()) {
            for (String seq : sequences) {
                // DDL Statement. No support in QueryDSL anymore. Have to do it the old-fashioned way.
                String sql = "DROP SEQUENCE IF EXISTS \"" + seq + "\"";
                try (PreparedStatement stmt = conn.prepareStatement(sql)) {
                    stmt.execute();
                    stmt.close();
                }
            }
            if(!conn.getAutoCommit()) {
                conn.commit();
            }
        }
    }

    /**
     * Returns the count of tables and sequences
     * this map depends on
     */
    public static long getMapTableSeqCount(long mapId) throws SQLException {
        long count = 0;

        try (Connection conn = getConnection()) {
            String sql = "SELECT count(*) " +
                    "FROM information_schema.tables " +
                    "WHERE table_schema='public' AND table_name LIKE " + "'%_" + mapId + "'";

            try (PreparedStatement stmt = conn.prepareStatement(sql)) {
                try (ResultSet rs = stmt.executeQuery()) {
                    if (rs.next())
                        count += rs.getLong(1);
                    stmt.close();
                }
            }


            sql = "SELECT count(*) " +
                    "FROM information_schema.sequences " +
                    "WHERE sequence_schema='public' AND sequence_name LIKE " + "'%_" + mapId + "_id_seq'";

            try (PreparedStatement stmt = conn.prepareStatement(sql)) {
                try (ResultSet rs = stmt.executeQuery()) {
                    if (rs.next())
                        count += rs.getLong(1);
                    stmt.close();
                }
            }
        }

        return count;
    }

    /*
        SELECT id
        FROM maps
        WHERE (tags -> 'lastAccessed' IS NULL AND created_at < ts)
        OR TO_TIMESTAMP(tags -> 'lastAccessed', 'YYYY-MM-DD"T"HH24:MI:SS.MS"Z"') < ts

     */
    private static BooleanExpression getStale(Timestamp ts) {
        return (Expressions.stringTemplate("tags->'lastAccessed'").isNull().and(maps.createdAt.lt(ts)))
                .or(Expressions.dateTimeTemplate(Timestamp.class, "TO_TIMESTAMP(tags -> 'lastAccessed', 'YYYY-MM-DD\"T\"HH24:MI:SS.MS\"Z\"')").lt(ts));
    }

    public static List<Maps> getStaleMaps(Timestamp ts) {
        return createQuery()
                .select(maps)
                .from(maps)
                .where(getStale(ts))
                .fetch();
    }

    public static Map<String, Long> getStaleMapsSummary(Timestamp ts) {
        List<Tuple> result =  createQuery()
                .select(users.displayName, maps.countDistinct())
                .from(users, maps)
                .where(users.id.eq(maps.userId).and(getStale(ts)))
                .groupBy(users.displayName)
                .fetch();

        Map<String, Long> response = new HashMap<>();

        result.stream().forEach(tuple -> {
            response.put(tuple.get(users.displayName), tuple.get(maps.countDistinct()));
        });

        return response;
    }

    /**
     * Returns table size in bytes
     */
    public static long getTableSizeInBytes(String tableName) {
        return createQuery()
                .select(Expressions.numberTemplate(Long.class, "pg_total_relation_size('" + tableName + "')"))
                .from()
                .fetchOne();
    }

    /**
     * Checks for the existence of map
     *
     * @param mapName
     * @return returns true when exists else false
     */
    public static boolean mapExists(String mapName) {
        long id;
        try {
            id = getRecordIdForInputString(mapName, maps, maps.id, maps.displayName);
        } catch (WebApplicationException ex) {
            id = -1;
        }
        return (id > -1);
    }

    /**
     * Checks for the existence of user
     *
     * @param userId
     * @return returns true when exists else false
     */
    public static boolean userExists(Long userId) {
        return createQuery()
                .select(users)
                .from(users)
                .where(users.id.eq(userId))
                .fetchCount() == 1;
    }

    public static Users getUser(Long id) {
        return createQuery().select(users).from(users).where(users.id.eq(id)).fetchFirst();
    }

    /**
     * Returns the record ID associated with the record request input string for
     * the given DAO type. First attempts to parse the request string as a
     * record ID. If that is unsuccessful, it treats the request string as a
     * record display name. This currently only supports Map and User types.
     *
     * @param input
     *            can be either a map ID or a map name
     * @return if a record ID string is passed in, it is verified and returned;
     *         if a record name string is passed in, it is verified that only
     *         one record of the requested type exists with the given name, and
     *         its ID is returned
     */
    public static long getRecordIdForInputString(String input, RelationalPathBase<?> table,
            NumberPath<Long> idField, StringPath nameField) throws WebApplicationException {

        if (org.apache.commons.lang3.StringUtils.isEmpty(input)) {
            throw new BadRequestException("No record exists with ID: " + input
                    + ".  Please specify a valid record.");
        }

        // Check if we can compare by ID
        if (StringUtils.isNumeric(input)) {
            logger.debug("Verifying that record with ID = {} in '{}' table has previously been created ...",
                    input, table.getTableName());

            long recordCount = createQuery()
                    .from(table)
                    .where(idField.eq(Long.valueOf(input)))
                    .fetchCount();

            if (recordCount == 0) {
                throw new NotFoundException("No record exists with ID = " + input +
                        " in '" + table + "' table. Please specify a valid record.");
            }

            if (recordCount == 1) {
                return Long.valueOf(input);
            }

            if (recordCount > 1) {
                throw new BadRequestException("Multiple records exist with ID = " + input + " in '" + table
                        + "' table. Please specify a single, valid record.");
            }
        }
        else { // input wasn't parsed as a numeric ID, so let's try it as a name
            logger.debug("Verifying that record with NAME = {} in '{}' table has previously been created ...",
                    input, table.getTableName());

            // there has to be a better way to do this against the generated
            // code but haven't been able to get it to work yet
            List<Long> records = createQuery()
                    .select(idField)
                    .from(table)
                    .where(nameField.eq(input))
                    .fetch();

            if (records.isEmpty()) {
                throw new NotFoundException("No record exists with NAME = " + input +
                        " in '" + table + "' table. Please specify a valid record.");
            }

            if (records.size() == 1) {
                return records.get(0);
            }

            if (records.size() > 1) {
                throw new BadRequestException("Multiple records exist with NAME = " + input
                        + " in '" + table + "' table. Please specify a single, valid record.");
            }
        }

        return -1;
    }

    public static String getJobIdByTask(String taskInfo) {
        String jobId = createQuery()
                .select(jobStatus.jobId)
                .from(jobStatus)
                .where(Expressions.booleanTemplate("tags->'taskInfo' like 'taskingManager:" + taskInfo + "'"))
                .orderBy(jobStatus.start.desc())
                .fetchFirst();

        return jobId;
    }

    public static String getJobBounds(String jobId) {
        String foundId = createQuery()
            .select(Expressions.stringTemplate("tags->'bounds'"))
            .from(jobStatus)
            .where(jobStatus.jobId.eq(jobId))
            .fetchFirst();

        if (foundId == null) {
            foundId = createQuery()
                .select(Expressions.stringTemplate("tags->'bbox'"))
                .from(jobStatus)
                .where(jobStatus.jobId.eq(jobId))
                .fetchFirst();
        }

        return foundId;
    }

    public static List<Long> getTimeoutTasks(String projectId) {
        List<String> list = createQuery()
            .select(Expressions.stringTemplate("tags->'taskInfo'"))
            .from(jobStatus)
            .where(Expressions.booleanTemplate("exist(tags,'timeout')")
                    .and(Expressions.booleanTemplate("tags->'taskInfo' like 'taskingManager:" + projectId + "%'")))
            .fetch();

        String patternString = String.format("taskingManager:%s_([0-9]*)", projectId);
        Pattern pattern = Pattern.compile(patternString);
        List<Long> taskList = new ArrayList<>();
        for (String taskTag: list) {
            Matcher matcher = pattern.matcher(taskTag);
            boolean found = matcher.find();
            long taskId = -1;
            if (found) {
                taskId = Long.parseLong(matcher.group(1));
            }

            taskList.add(taskId);
        }

        return taskList;
    }

    public static Map<String, String> getJobStatusTags(String jobId) {
        Object jobTags = createQuery().select(jobStatus.tags)
                .from(jobStatus)
                .where(jobStatus.jobId.eq(jobId))
                .fetchOne();

        return PostgresUtils.postgresObjToHStore(jobTags);
    }

    public static void tagTimeoutTask(String jobId) {
        Map<String, String> tags = getJobStatusTags(jobId);
        tags.put("timeout", "true");

        SQLQueryFactory query = createQuery();
        query.update(jobStatus)
            .where(jobStatus.jobId.eq(jobId))
            .set(jobStatus.tags, tags)
            .execute();

        try {
            query.getConnection().commit();
        }
        catch (SQLException throwables) {
            throwables.printStackTrace();
        }
    }

    public static void removeTimeoutTag(String taskInfo) {
        SQLQueryFactory query = createQuery();
        query.update(jobStatus)
            .where(Expressions.booleanTemplate("tags->'taskInfo' like 'taskingManager:" + taskInfo + "'"))
            .set(Arrays.asList(jobStatus.tags), Arrays.asList(Expressions.stringTemplate("delete(tags, 'timeout')")))
            .execute();

        try {
            query.getConnection().commit();
        }
        catch (SQLException throwables) {
            throwables.printStackTrace();
        }
    }

    // Sets the specified job to a status detail of stale and recurses up to the parent jobs to do the same
    public static void setStale(String jobId) {
        if (jobId != null) {
            // Find the job
            JobStatus job = createQuery()
                .select(jobStatus)
                .from(jobStatus)
                .where(jobStatus.jobId.eq(jobId))
                .fetchFirst();

            if(job != null) {
                createQuery()
                    .update(jobStatus)
                    .where(jobStatus.jobId.eq(jobId))
                    .set(jobStatus.statusDetail, "STALE")
                    .execute();

                Map<String, String> tags = PostgresUtils.postgresObjToHStore(job.getTags());

                String parentId = tags.get("parentId");
                if (parentId != null) {
                    ArrayList<String> parentIds = new ArrayList<>(Arrays.asList(parentId.split(",")));

                    // If it has parent(s), make the parent(s) stale too
                    for(String id : parentIds) {
                        if (id != null) {
                            setStale(id);
                        }
                    }
                }
            }
        }
    }

    private static final FilenameFilter filter = new FilenameFilter() {
        @Override
        public boolean accept(File f, String name) {
            // We want to find only OsmApiWriter*.osc files
            return name.startsWith("OsmApiWriter") && name.endsWith(".osc");
        }
    };

    // Sets the specified upload changeset job to a status detail of CONFLICTS
    // if a diff-error.osc file is present in the parent job workspace
    public static void checkConflicted(String jobId, String parentId) {
        if (parentId != null) {
            File workDir = new File(CHANGESETS_FOLDER, parentId);
            File diffError = new File(workDir, "diff-error.osc");
            File diffRemaining = new File(workDir, "diff-remaining.osc");
            File[] diffDebugfiles = workDir.listFiles(filter);
            if (diffError.exists()
                    || diffRemaining.exists()
                    || diffDebugfiles.length > 0
            ) {
                // Find the job
                JobStatus job = createQuery()
                        .select(jobStatus)
                        .from(jobStatus)
                        .where(jobStatus.jobId.eq(jobId))
                        .fetchFirst();

                if(job != null) {
                    createQuery()
                            .update(jobStatus)
                            .where(jobStatus.jobId.eq(jobId))
                            .set(jobStatus.statusDetail, "CONFLICTS")
                            .execute();
                }
            }
        }
    }

    /**
     * Inserts the command_status if it doesn't exist already, else update the stdout, stderr, and percent_complete for the command
     * This function will also call updateJobProgress
     * @param commandResult
     */
    public static void upsertCommandStatus(CommandResult commandResult) {
        ResultSet queryResult;

        try (Connection conn = getConnection(); Statement dbQuery = conn.createStatement()) {
            if(commandResult.getId() == null) {
                String queryInsert = String.format(
                        "INSERT INTO command_status(start, command, job_id, stdout, stderr, percent_complete) " +
                        "VALUES('%s', '%s', '%s', '%s', '%s', '%d') ",
                        commandResult.getStart(),
                        commandResult.getCommand(),
                        commandResult.getJobId(),
                        commandResult.getStdout(),
                        commandResult.getStderr(),
                        commandResult.getPercentProgress());

                dbQuery.executeUpdate(queryInsert, Statement.RETURN_GENERATED_KEYS);
                queryResult = dbQuery.getGeneratedKeys();

                if (queryResult.next()) {
                    Long id = queryResult.getLong(1);
                    commandResult.setId(id);
                }
            }
            else {
                String queryUpdate = String.format(
                        "UPDATE command_status " +
                        "SET stdout = concat(stdout, '%s'), stderr = concat(stderr, '%s'), percent_complete = '%d' " +
                        "WHERE id=%d",
                        commandResult.getStdout(), commandResult.getStderr(), commandResult.getPercentProgress(), commandResult.getId());

                dbQuery.executeUpdate(queryUpdate);
            }

            updateJobProgress(commandResult.getJobId());

            if (!conn.getAutoCommit()) {
                conn.commit();
            }
        }
        catch(SQLException exc) {
            logger.error(exc.getMessage());
        }
    }

    /**
     * Sets exit code and finish time for the specified command
     * @param commandResult
     */
    public static void completeCommandStatus(CommandResult commandResult) {
        try (Connection conn = getConnection(); Statement dbQuery = conn.createStatement()) {
            String queryUpdate = String.format(
                    "UPDATE command_status " +
                    "SET exit_code = '%d', finish = '%s' " +
                    "WHERE id=%d",
                    commandResult.getExitCode(), commandResult.getFinish(), commandResult.getId());

            dbQuery.executeUpdate(queryUpdate);

            if (!conn.getAutoCommit()) {
                conn.commit();
            }
        }
        catch(SQLException exc) {
            logger.error(exc.getMessage());
        }
    }

    /**
     * Updates the percent_complete for the job matching the specified jobId
     * @param jobId
     */
    public static void updateJobProgress(String jobId) {
        ResultSet queryResult;

        try (Connection conn = getConnection(); Statement dbQuery = conn.createStatement()) {
            // Get count for commands that have completed
            String completedCommandsQuery = String.format("SELECT count(*) AS total FROM command_status WHERE exit_code = 0 and job_id = '%s'", jobId);
            queryResult = dbQuery.executeQuery(completedCommandsQuery);
            int completedCount = -1;
            if(queryResult.next()) {
                completedCount = queryResult.getInt("total");
            }

            // Get current running commands percent completed
            String currentCommandQuery = String.format("SELECT percent_complete AS percent FROM command_status WHERE exit_code is null and job_id = '%s'", jobId);
            queryResult = dbQuery.executeQuery(currentCommandQuery);
            int currentCommandPercent = -1;
            if(queryResult.next()) {
                currentCommandPercent = queryResult.getInt("percent");
            }

            // Get total number of commands for the job
            String totalCommandsQuery = String.format("SELECT trackable_command_count AS total, percent_complete AS currentPercent FROM job_status WHERE job_id = '%s'", jobId);
            queryResult = dbQuery.executeQuery(totalCommandsQuery);
            int totalCommandCount = 0;
            int oldProgress = 0;
            if(queryResult.next()) {
                totalCommandCount = queryResult.getInt("total");
                oldProgress = queryResult.getInt("currentPercent");
            }

            // check that some value was returned that isnt the default. total command count must be > 0
            if(completedCount > -1 && currentCommandPercent > -1 && totalCommandCount > 0) {
                int currentJobProgress = (((completedCount * 100) + currentCommandPercent) / totalCommandCount);

                // Helps avoid redundant sql updates
                if(currentJobProgress != oldProgress) {
                    String queryUpdate = String.format(
                            "UPDATE job_status SET percent_complete = '%d' WHERE job_id = '%s'",
                            currentJobProgress, jobId);

                    dbQuery.executeUpdate(queryUpdate);

                    if (!conn.getAutoCommit()) {
                        conn.commit();
                    }
                }
            }
        }
        catch(SQLException exc) {
            logger.error(exc.getMessage());
        }
    }

    /**
     * Retrieves the percent_complete for the job matching the specified jobId
     * @param jobId
     * @return
     */
    public static Integer getJobProgress(String jobId) {
        int progress = 0;
        ResultSet queryResult;

        try (Connection conn = getConnection(); Statement dbQuery = conn.createStatement()) {
            String queryUpdate = String.format("SELECT percent_complete AS percent_complete from job_status WHERE job_id = '%s'", jobId);

            queryResult = dbQuery.executeQuery(queryUpdate);
            if(queryResult.next()) {
                progress = queryResult.getInt("percent_complete");
            }
        }
        catch(SQLException exc) {
            logger.error("ERROR HERE: " + exc.getMessage());
        }

        return progress;
    }

    public static void deleteOSMRecordById(Long mapId) {
        deleteMapRelatedTablesByMapId(mapId);
        DbUtils.deleteMap(mapId);
    }

    public static void deleteBookmarksByMapId(Long mapId) {
        createQuery().delete(reviewBookmarks).where(reviewBookmarks.mapId.eq(mapId)).execute();
    }

    public static Map<String, String> getLastPushedInfo(String jobId) {
        Map<String, String> elementInfo = new HashMap<>();

        if (jobId != null) {
            String stdOutWithId = createQuery()
                .select(commandStatus.stdout)
                .from(commandStatus)
                .where(commandStatus.stdout.like("%Last element pushed:%").and(commandStatus.jobId.eq(jobId)))
                .fetchFirst();

            if (stdOutWithId == null) return elementInfo;

            String patternString = "Last element pushed: Type\\((\\w+)\\) (\\w+)\\(([0-9]+)\\) Version\\(([0-9]+)\\)";

            Pattern pattern = Pattern.compile(patternString);
            Matcher matcher = pattern.matcher(stdOutWithId);
            boolean found = matcher.find();

            if (found) {
                elementInfo.put("operationType", matcher.group(1));
                elementInfo.put("featureType", matcher.group(2));
                elementInfo.put("featureId", matcher.group(3));
                elementInfo.put("version", matcher.group(4));
            }
        }

        return elementInfo;
    }

    public static String getStdoutStats(String jobId) {
        List<String> stats = new ArrayList<>();
        if (jobId != null) {
            String stdOutWithId = createQuery()
                .select(commandStatus.stdout)
                .from(commandStatus)
                .where(commandStatus.jobId.eq(jobId))
                .fetchFirst();

            if (stdOutWithId == null) return "";

            Scanner scanner = new Scanner(stdOutWithId);
            boolean capture = false;
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine();
                int idx;
                if ((idx = line.indexOf("stats = ")) > -1) {
                    capture = true;
                    line = line.substring(idx);
                }
                if (line.isEmpty()) {
                    break;
                }
                if (capture) {
                    stats.add(line);
                }
            }
            scanner.close();
        }

        return String.join("\n", stats);
    }

    public static boolean didChangesetsUpload(String jobId) {

        if (jobId != null) {
            Long noChangesetsUploaded = createQuery()
                .selectFrom(commandStatus)
                .where(commandStatus.stdout.like("%Total OSM Changesets Uploaded\t0%").and(commandStatus.jobId.eq(jobId)))
                .fetchCount();

            if (noChangesetsUploaded == 0) {
                return true;
            }
            return false;
        }

        return true;
    }

    public static List<TranslationFolder> getTranslationFoldersForUser(Long userId) {
        SQLQuery<TranslationFolder> sql = createQuery()
                .select(translationFolders)
                .from(translationFolders)
                .where(translationFolders.id.ne(0L));

        if (userId != null && !UserResource.adminUserCheck(getUser(userId))) {
            sql.where(
                    translationFolders.userId.eq(userId).or(translationFolders.publicCol.isTrue())
            );
        }

        List<TranslationFolder> folderRecordSet = sql.orderBy(translationFolders.displayName.asc()).fetch();

        return folderRecordSet;
    }

    public static Set<Long> getTranslationFolderIdsForUser(Long userId) {
        List<TranslationFolder> folders = getTranslationFoldersForUser(userId);
        Set<Long> out = new HashSet<Long>(folders.size());
        for (TranslationFolder f : folders) {
            out.add(f.getId());
        }
        return out;
    }

    public static void addTranslation(String scriptName, Long userId, Long folderId) {
        Boolean isPublic = createQuery()
            .select(translationFolders.publicCol)
            .from(translationFolders)
            .where(translationFolders.id.eq(folderId))
            .fetchOne();

        Timestamp created = new Timestamp(System.currentTimeMillis());

        createQuery()
                .insert(translations)
                .columns(translations.displayName, translations.userId, translations.publicCol, translations.createdAt, translations.folderId)
                .values(scriptName, userId, isPublic, created, folderId)
                .execute();
    }

    public static Translations getTranslation(Long translationId) {
        return createQuery().select(translations)
                .from(translations)
                .where(translations.id.eq(translationId))
                .fetchFirst();
    }

    public static TranslationFolder getTranslationFolder(Long folderId) {
        return createQuery()
            .select(translationFolders)
            .from(translationFolders)
            .where(translationFolders.id.eq(folderId))
            .fetchFirst();
    }

    public static void changeTranslationFoldersPath(Long folderId, String newPath) {
        // Set new path for folderId
        createQuery()
            .update(translationFolders)
            .where(translationFolders.id.eq(folderId))
            .set(translationFolders.path, newPath)
            .execute();

        List<TranslationFolder> folders = createQuery()
            .select(translationFolders)
            .from(translationFolders)
            .where(translationFolders.parentId.eq(folderId))
            .fetch();

        for (TranslationFolder folder : folders) {
            String path = newPath + File.separator + folder.getDisplayName();

            changeTranslationFoldersPath(folder.getId(), path);
        }
    }

}
