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
 * @copyright Copyright (C) 2016, 2017, 2021 Maxar (http://www.maxar.com/)
 */
package hoot.services.utils;

import static hoot.services.HootProperties.HOME_FOLDER;
import static hoot.services.HootProperties.UPLOAD_FOLDER;
import static org.junit.Assert.fail;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.UUID;

import org.apache.commons.io.FileUtils;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.experimental.categories.Category;

import hoot.services.UnitTest;


public class MultipartSerializerTest {
    private static File homefolder;
    private static String original_HOME_FOLDER;
    private static String original_UPLOAD_FOLDER;

    @BeforeClass
    public static void oneTimeSetup() throws Exception {
        original_HOME_FOLDER = HOME_FOLDER;
        original_UPLOAD_FOLDER = UPLOAD_FOLDER;
        homefolder = new File(FileUtils.getTempDirectory(), "MultipartSerializerTest");
        FileUtils.forceMkdir(homefolder);
        Assert.assertTrue(homefolder.exists());
        HootCustomPropertiesSetter.setProperty("HOME_FOLDER", homefolder.getAbsolutePath());
        HootCustomPropertiesSetter.setProperty("UPLOAD_FOLDER", homefolder.getAbsolutePath() + "/" + "upload");
    }

    @AfterClass
    public static void afterClass() throws Exception {
        FileUtils.deleteDirectory(homefolder);
        HootCustomPropertiesSetter.setProperty("HOME_FOLDER", original_HOME_FOLDER);
        HootCustomPropertiesSetter.setProperty("UPLOAD_FOLDER", original_UPLOAD_FOLDER);
    }

    @Test
    @Category(UnitTest.class)
    public void testSerializeFGDB() throws Exception {
        String jobId = UUID.randomUUID().toString();
        File workingDir = new File(UPLOAD_FOLDER + File.separator + jobId);
        FileUtils.forceMkdir(workingDir);
        Assert.assertTrue(workingDir.exists());

        try {
/*            String fileName = "fgdbTest.gdb";
            String textFieldValue = "GDB content is ignored";
            File out = new File(workingDir, fileName);
            FileUtils.write(out, textFieldValue);

            // MediaType of the body part will be derived from the file.
//            FileDataBodyPart filePart = new FileDataBodyPart();
//            filePart.setName("fgdbtest");
//            filePart.setFileEntity(out, MediaType.APPLICATION_OCTET_STREAM_TYPE);

            MIMEMessage mimeMessage = new MIMEMessage(new FileInputStream(out), "");
            MIMEPart mimePart = mimeMessage.getPart(0);
            BodyPartEntity bodyPartEntity = new BodyPartEntity(mimePart);

            BodyPart bodyPart = new BodyPart(bodyPartEntity, MediaType.APPLICATION_OCTET_STREAM_TYPE);
            FormDataContentDisposition formDataContentDisposition =
                    new FormDataContentDisposition("form-data; name=\"eltuploadfile0\"; filename=\"" + fileName + "\"");
            bodyPart.setContentDisposition(formDataContentDisposition);
            MessageBodyWorkers messageBodyWorkers = new MessageBodyWorkers() {
                @Override
                public Map<MediaType, List<MessageBodyReader>> getReaders(MediaType mediaType) {
                    return null;
                }

                @Override
                public Map<MediaType, List<MessageBodyWriter>> getWriters(MediaType mediaType) {
                    return null;
                }

                @Override
                public String readersToString(Map<MediaType, List<MessageBodyReader>> readers) {
                    return null;
                }

                @Override
                public String writersToString(Map<MediaType, List<MessageBodyWriter>> writers) {
                    return null;
                }

                @Override
                public <T> MessageBodyReader<T> getMessageBodyReader(Class<T> type, Type genericType, Annotation[] annotations, MediaType mediaType) {
                    return (MessageBodyReader<T>) new InputStreamProvider();
                }

                @Override
                public <T> MessageBodyReader<T> getMessageBodyReader(Class<T> type, Type genericType, Annotation[] annotations, MediaType mediaType, PropertiesDelegate propertiesDelegate) {
                    return null;
                }

                @Override
                public <T> MessageBodyWriter<T> getMessageBodyWriter(Class<T> type, Type genericType, Annotation[] annotations, MediaType mediaType) {
                    return null;
                }

                @Override
                public <T> MessageBodyWriter<T> getMessageBodyWriter(Class<T> type, Type genericType, Annotation[] annotations, MediaType mediaType, PropertiesDelegate propertiesDelegate) {
                    return null;
                }

                @Override
                public List<MediaType> getMessageBodyReaderMediaTypes(Class<?> type, Type genericType, Annotation[] annotations) {
                    return null;
                }

                @Override
                public List<MediaType> getMessageBodyReaderMediaTypesByType(Class<?> type) {
                    return null;
                }

                @Override
                public List<MessageBodyReader> getMessageBodyReadersForType(Class<?> type) {
                    return null;
                }

                @Override
                public List<ReaderModel> getReaderModelsForType(Class<?> type) {
                    return null;
                }

                @Override
                public List<MediaType> getMessageBodyWriterMediaTypes(Class<?> type, Type genericType, Annotation[] annotations) {
                    return null;
                }

                @Override
                public List<MediaType> getMessageBodyWriterMediaTypesByType(Class<?> type) {
                    return null;
                }

                @Override
                public List<MessageBodyWriter> getMessageBodyWritersForType(Class<?> type) {
                    return null;
                }

                @Override
                public List<WriterModel> getWritersModelsForType(Class<?> type) {
                    return null;
                }

                @Override
                public MediaType getMessageBodyWriterMediaType(Class<?> type, Type genericType, Annotation[] annotations, List<MediaType> acceptableMediaTypes) {
                    return null;
                }

                @Override
                public Object readFrom(Class<?> rawType, Type type, Annotation[] annotations, MediaType mediaType, MultivaluedMap<String, String> httpHeaders, PropertiesDelegate propertiesDelegate, InputStream entityStream, Iterable<ReaderInterceptor> readerInterceptors, boolean translateNce) throws WebApplicationException, IOException {
                    return null;
                }

                @Override
                public OutputStream writeTo(Object entity, Class<?> rawType, Type type, Annotation[] annotations, MediaType mediaType, MultivaluedMap<String, Object> httpHeaders, PropertiesDelegate propertiesDelegate, OutputStream entityStream, Iterable<WriterInterceptor> writerInterceptors) throws IOException, WebApplicationException {
                    return null;
                }
            };
            bodyPart.setMessageBodyWorkers(messageBodyWorkers);

            FormDataMultiPart formDataMultiPart = new FormDataMultiPart();
            formDataMultiPart.bodyPart(bodyPart);

            Map<String, String> uploadedFiles = new HashMap<>();
            Map<String, String> uploadedFilesPaths = new HashMap<>();

            MultipartSerializer.serializeUpload(jobId, "DIR", uploadedFiles, uploadedFilesPaths, formDataMultiPart);

            Assert.assertEquals("GDB", uploadedFiles.get("fgdbTest"));
            Assert.assertEquals("fgdbTest.gdb", uploadedFilesPaths.get("fgdbTest"));

            File fgdbpath = new File(workingDir, "fgdbTest.gdb");
            Assert.assertTrue(fgdbpath.exists());

            File content = new File(workingDir, "/fgdbTest.gdb/dummy1.gdbtable");
            Assert.assertTrue(content.exists());
*/
        }
        finally {
            FileUtils.forceDelete(workingDir);
        }
    }

    @Test
    @Category(UnitTest.class)
    public void testSerializeUploadedFiles() throws Exception {
        String jobId = UUID.randomUUID().toString();
        File workingDir = new File(UPLOAD_FOLDER + File.separator + jobId);
        FileUtils.forceMkdir(workingDir);
        Assert.assertTrue(workingDir.exists());

        try {
/*
            String fileName = "test.osm";
            String someValue = "OSM content is ignored";
            File out = new File(workingDir, fileName);
            FileUtils.write(out, someValue);

            // MediaType of the body part will be derived from the file.
            FileDataBodyPart filePart = new FileDataBodyPart("", out, MediaType.APPLICATION_OCTET_STREAM_TYPE);
            FormDataContentDisposition formDataContentDisposition =
                    new FormDataContentDisposition("form-data; name=\"eltuploadfile0\"; filename=\"" + fileName + "\"");
            filePart.setContentDisposition(formDataContentDisposition);
            FormDataMultiPart multiPart = new FormDataMultiPart();
            multiPart.bodyPart(filePart);

            Map<String, String> uploadedFiles = new HashMap<>();
            Map<String, String> uploadedFilesPaths = new HashMap<>();

            MultipartSerializer.serializeUpload(jobId, "FILE", uploadedFiles, uploadedFilesPaths, multiPart);

            Assert.assertEquals("OSM", uploadedFiles.get("test"));
            Assert.assertEquals("test.osm", uploadedFilesPaths.get("test"));
*/
        }
        finally {
            FileUtils.forceDelete(workingDir);
        }
    }

    @Test
    @Category(UnitTest.class)
    public void testValidatePath() throws Exception {
        Method m = MultipartSerializer.class.getDeclaredMethod("validatePath", File.class, File.class);
        m.setAccessible(true);

        //use null if the method is static
        m.invoke(null, new File("/projects/hoot/upload/123456"), new File("/projects/hoot/upload/123456/DcGisRoads.gdb"));

        try {
            m.invoke(null, new File("/projects/hoot/upload/123456"), new File("/projects/hoot/upload/123456/../DcGisRoads.gdb"));
            fail("Invalid path should've been detected");
        }
        catch (InvocationTargetException ite) {
            if (! (ite.getCause() instanceof RuntimeException)) {
                fail("Expected RuntimeException!");
            }
        }

        try {
            m.invoke(null, new File("/projects/hoot/upload/123456"), new File("\0//DcGisRoads.gdb"));
            fail("Invalid path should've been detected");
        }
        catch (InvocationTargetException ite) {
            if (! (ite.getCause() instanceof RuntimeException)) {
                fail("Expected RuntimeException!");
            }
        }
    }
}
