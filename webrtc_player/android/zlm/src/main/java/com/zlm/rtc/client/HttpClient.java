package com.zlm.rtc.client;

import java.io.*;
import java.net.*;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class HttpClient {

    //MIME部分文件类型对照表
    private static final Map<String, String> FILE_TYPE = new HashMap<>();

    static {
        FILE_TYPE.put(".jpeg", "image/jpeg");
        FILE_TYPE.put(".jpg", "image/jpg");
        FILE_TYPE.put(".png", "image/png");
        FILE_TYPE.put(".bmp", "image/bmp");
        FILE_TYPE.put(".gif", "image/gif");
        FILE_TYPE.put(".mp4", "video/mp4");
        FILE_TYPE.put(".txt", "text/plain");
        FILE_TYPE.put(".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
        FILE_TYPE.put(".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document");
        FILE_TYPE.put(".pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation");
        FILE_TYPE.put(".pdf", "application/pdf");
    }

    /**
     * GET请求
     *
     * @param url
     * @param params
     * @param headers
     * @return
     */
    public static String doGet(String url, Map<String, String> params, Map<String, String> headers) {
        BufferedReader reader = null;
        try {
            //1、拼接url
            StringBuffer stringBuffer = new StringBuffer(url);
            if (params != null && !params.isEmpty()) {
                stringBuffer.append("?");
                for (Map.Entry<String, String> entry : params.entrySet()) {
                    stringBuffer.append(entry.getKey()).append("=").append(entry.getValue()).append("&");
                }
                stringBuffer.deleteCharAt(stringBuffer.length() - 1);
            }
            URL testUrl = new URL(stringBuffer.toString());

            //2、建立链接
            HttpURLConnection connection = (HttpURLConnection) testUrl.openConnection();
            connection.setConnectTimeout(3000); //设置连接超时
            connection.setReadTimeout(3000); //设置读取响应超时
            if (headers != null && !headers.isEmpty()) {
                for (Map.Entry<String, String> entry : headers.entrySet()) {
                    connection.setRequestProperty(entry.getKey(), entry.getValue());
                }
            }

            //3、发送请求
            InputStream inputStream = connection.getInputStream();
            reader = new BufferedReader(new InputStreamReader(inputStream));
            String line = "";
            StringBuffer response = new StringBuffer();
            while ((line = reader.readLine()) != null) {
                response.append(line);
            }
            reader.close();
            return response.toString();

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    System.out.println("输入流关闭失败");
                }
            }
        }
        return null;
    }

    /**
     * POST请求
     *
     * @param url
     * @param params
     * @param headers
     * @return
     */
    public static String doPost(String url, Map<String, String> params, Map<String, String> headers) {
        OutputStream outputStream = null;
        BufferedReader reader = null;
        try {
            //建立连接
            URL testUrl = new URL(url);
            HttpURLConnection connection = (HttpURLConnection) testUrl.openConnection();
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);   //允许写入输出流
            connection.setUseCaches(false); //禁用缓存
            connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
            if (headers != null && !headers.isEmpty()) {
                for (Map.Entry<String, String> entry : headers.entrySet()) {
                    connection.setRequestProperty(entry.getKey(), entry.getValue());
                }
            }

            //写入请求体
            outputStream = connection.getOutputStream();
            StringBuffer payload = new StringBuffer();
            if (params != null && !params.isEmpty()) {
                for (Map.Entry<String, String> entry : params.entrySet()) {
                    payload.append(entry.getKey()).append("=").append(entry.getValue()).append("&");
                }
                payload.deleteCharAt(payload.length() - 1);
            }
            outputStream.write(payload.toString().getBytes());
            outputStream.flush();
            outputStream.close();

            //发送请求
            InputStream inputStream = connection.getInputStream();
            reader = new BufferedReader(new InputStreamReader(inputStream));
            String line = "";
            StringBuffer response = new StringBuffer();
            while ((line = reader.readLine()) != null) {
                response.append(line);
            }
            reader.close();
            return response.toString();

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    System.out.println("输出流关闭失败");
                }
            }
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    System.out.println("输入流关闭失败");
                }
            }
        }
        return null;
    }

    /**
     * GET请求下载文件
     *
     * @param url
     * @param params
     * @param headers
     * @param filePath
     */
    public static void doGetDownload(String url, Map<String, String> params, Map<String, String> headers, String filePath) {
        BufferedInputStream inputStream = null;
        FileOutputStream outputStream = null;
        try {
            //1、拼接url
            StringBuffer stringBuffer = new StringBuffer(url);
            if (params != null && !params.isEmpty()) {
                stringBuffer.append("?");
                for (Map.Entry<String, String> entry : params.entrySet()) {
                    stringBuffer.append(entry.getKey()).append("=").append(entry.getValue()).append("&");
                }
                stringBuffer.deleteCharAt(stringBuffer.length() - 1);
            }
            URL testUrl = new URL(stringBuffer.toString());

            //2、建立链接
            HttpURLConnection connection = (HttpURLConnection) testUrl.openConnection();
            connection.setConnectTimeout(3000); //设置连接超时
            connection.setReadTimeout(3000); //设置读取响应超时
            if (headers != null && !headers.isEmpty()) {
                for (Map.Entry<String, String> entry : headers.entrySet()) {
                    connection.setRequestProperty(entry.getKey(), entry.getValue());
                }
            }

            //3、发送请求
            inputStream = new BufferedInputStream(connection.getInputStream());
            String contentDisposition = connection.getHeaderField("Content-Disposition");
            String regex = "attachment; filename=(.+\\.\\w+)";
            Pattern pattern = Pattern.compile(regex);
            Matcher matcher = pattern.matcher(contentDisposition);
            if (matcher.find()) {
                String fileName = matcher.group(1);
                File file = new File(filePath + "\\" + fileName);
                outputStream = new FileOutputStream(file);
                int n;
                while ((n = inputStream.read()) != -1) {
                    outputStream.write(n);
                }
                outputStream.flush();
                outputStream.close();
            }
            inputStream.close();

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    System.out.println("输出流关闭失败");
                }
            }
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException e) {
                    System.out.println("输入流关闭失败");
                }
            }
        }
    }

    /**
     * POST请求上传文件
     *
     * @param url
     * @param fileUrl
     * @param params
     * @param headers
     * @return
     */
    public static String doPostUpload(String url, String fileUrl, Map<String, String> params, Map<String, String> headers) {
        FileInputStream fileInputStream = null;
        OutputStream outputStream = null;
        BufferedReader reader = null;
        try {
            //读文件
            File file = new File(fileUrl);
            fileInputStream = new FileInputStream(file);
            byte[] bytes = new byte[(int) file.length()];
            fileInputStream.read(bytes);
            fileInputStream.close();

            URL testUrl = new URL(url);
            HttpURLConnection connection = (HttpURLConnection) testUrl.openConnection();
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);   //允许写入输出流
            connection.setUseCaches(false); //禁用缓存
            String boundary = UUID.randomUUID().toString();
            connection.setRequestProperty("Content-Type", "multipart/form-data; boundary=" + boundary);
            if (headers != null && !headers.isEmpty()) {
                for (Map.Entry<String, String> entry : headers.entrySet()) {
                    connection.setRequestProperty(entry.getKey(), entry.getValue());
                }
            }

            //写入请求体
            outputStream = connection.getOutputStream();

            StringBuffer start = new StringBuffer();
            start.append("--").append(boundary).append("\r\n");
            String fileName = file.getName();
            String fileExtension = fileName.substring(fileName.lastIndexOf('.'));
            start.append("Content-Disposition: form-data; name=\"file\"; filename=").append(fileName).append("\r\n");
            start.append("Content-Type: ").append(FILE_TYPE.get(fileExtension)).append("\r\n\r\n");
            outputStream.write(start.toString().getBytes());
            outputStream.write(bytes);
            outputStream.write("\r\n".getBytes());

            StringBuffer mid = new StringBuffer();
            if (params != null && !params.isEmpty()) {
                for (Map.Entry<String, String> entry : params.entrySet()) {
                    mid.append("--").append(boundary).append("\r\n");
                    mid.append("Content-Disposition: form-data; name=\"").append(entry.getKey()).append("\"\r\n\r\n");
                    mid.append(entry.getValue()).append("\r\n");
                }
                outputStream.write(mid.toString().getBytes());
            }

            String end = "--" + boundary + "--";
            outputStream.write(end.getBytes());
            outputStream.flush();
            outputStream.close();

            //发送请求
            InputStream inputStream = connection.getInputStream();
            reader = new BufferedReader(new InputStreamReader(inputStream));
            String line = "";
            StringBuffer response = new StringBuffer();
            while ((line = reader.readLine()) != null) {
                response.append(line);
            }
            reader.close();
            return response.toString();

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (fileInputStream != null) {
                try {
                    fileInputStream.close();
                } catch (IOException e) {
                    System.out.println("文件流关闭失败");
                }
            }
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    System.out.println("输出流关闭失败");
                }
            }
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    System.out.println("输入流关闭失败");
                }
            }
        }
        return null;
    }

    /**
     * 从返回头中获取登录token
     *
     * @param url
     * @param params
     * @param headers
     * @return
     */
    public static String getToken(String url, Map<String, String> params, Map<String, String> headers) {
        OutputStream outputStream = null;
        try {
            //建立连接
            URL testUrl = new URL(url);
            HttpURLConnection connection = (HttpURLConnection) testUrl.openConnection();
            connection.setRequestMethod("POST");
            connection.setDoOutput(true);   //允许写入输出流
            connection.setUseCaches(false); //禁用缓存
            connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded");
            connection.setInstanceFollowRedirects(false); //禁用跟随重定向

            //写入请求体
            outputStream = connection.getOutputStream();
            StringBuffer payload = new StringBuffer();
            if (params != null && !params.isEmpty()) {
                for (Map.Entry<String, String> entry : params.entrySet()) {
                    payload.append(entry.getKey()).append("=").append(entry.getValue()).append("&");
                }
                payload.deleteCharAt(payload.length() - 1);
            }
            outputStream.write(payload.toString().getBytes());
            outputStream.flush();
            outputStream.close();

            //发送请求，重定向到返回头中的Location
            connection.connect();
            URL location = new URL(connection.getHeaderField("Location"));
            HttpURLConnection connection2 = (HttpURLConnection) location.openConnection();

            //请求Location，获取返回头中的所有Set-Cookie
            connection2.setRequestMethod("GET");
            connection2.setInstanceFollowRedirects(false);
            connection2.connect();
            List<String> cookies = connection2.getHeaderFields().get("Set-Cookie");
            for (String cookie : cookies) {
                if (cookie.contains("token-test=")) {
                    return cookie;
                }
            }

        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    System.out.println("输出流关闭失败");
                }
            }
        }
        return null;
    }
}


