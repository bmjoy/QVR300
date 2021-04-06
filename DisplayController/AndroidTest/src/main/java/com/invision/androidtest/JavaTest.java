package com.invision.androidtest;

public class JavaTest {
    public static void main(String[] args) {
        testVertex();
    }

    private static void testThread() {
        int i = 0;
        boolean again = true;

        System.out.println("start Test");

        while (again) {
            try {
                Thread.sleep(1000);
                System.out.println("waiting " + i + " seconds");
                i++;
                if (i == 5) {
                    again = false;
                }
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        System.out.println("start Test");
    }

    private static void testVertex() {
        float windowWidth = 1920;
        float windowHeight = 1080;

        int left = 0;
        int top = 0;
        int width = 1200;
        int height = 1080;

        int real_left = 0;
        int real_top = 716;
        int real_width = 1200;
        int real_height = 1080 + real_top;

        float offsetX = (windowWidth - width) / 2;
        float offsetY = (windowHeight - height) / 2;
        float oriLeft = (left + offsetX) / windowWidth;
        float oriRight = (left + width + offsetX) / windowWidth;
        float oriTop = (windowHeight - top - offsetY) / windowHeight;
        float oriBottom = (windowHeight - top - height - offsetY) / windowHeight;
        float y = (windowHeight - height ) / 2 / windowHeight;
        float oriWidth = width / windowWidth;
        float oriHeight = height / windowHeight;

        if (oriBottom < y) {
            oriBottom = y;
        }

        if (oriRight > 1) {
            oriRight = 1;
        }
        // float vertexLeft = oriLeft;
        float vertexLeft = (oriRight - oriLeft) * real_left / width + oriLeft;
        // float vertexRight = oriRight;
        float vertexRight = (oriRight - oriLeft) * real_width / width + oriLeft;
        // float vertexTop = oriTop;
        float vertexTop = (oriBottom - oriTop) * real_top / height + oriTop;
        // float vertexBottom = oriBottom;
        float vertexBottom = (oriBottom - oriTop) * real_height / height + oriTop;
        float aspectRatio = 1.0f;
        vertexTop = (1.0f - aspectRatio) / 2 + vertexTop * aspectRatio;
        vertexBottom = (1.0f - aspectRatio) / 2 + vertexBottom * aspectRatio;

        float xRatio = 0.378937f;
        float yRatio = 0.212992f;
        vertexLeft = (vertexLeft * 2 - 1) * xRatio;
        vertexRight = (vertexRight * 2 - 1) * xRatio;
        vertexTop = (vertexTop * 2 - 1) * yRatio;
        vertexBottom = (vertexBottom * 2 - 1) * yRatio;

        System.out.println("vertexLeft=" + String.format("%.6f", vertexLeft));
        System.out.println("vertexRight=" + String.format("%.6f", vertexRight));
        System.out.println("vertexTop=" + String.format("%.6f", vertexTop));
        System.out.println("vertexBottom=" + String.format("%.6f", vertexBottom));

        float[] vertexPosition = new float[12];

        vertexPosition[0] = vertexLeft;
        vertexPosition[1] = vertexBottom;
        vertexPosition[2] = 0;
        vertexPosition[3] = vertexRight;
        vertexPosition[4] = vertexBottom;
        vertexPosition[5] = 0;
        vertexPosition[6] = vertexLeft;
        vertexPosition[7] = vertexTop;
        vertexPosition[8] = 0;
        vertexPosition[9] = vertexRight;
        vertexPosition[10] = vertexTop;
        vertexPosition[11] = 0;
    }
}
