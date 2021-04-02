//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------
package com.qualcomm.svr.apps.photoViewer;


public class svrSphereMesh {

    public float[] sphereVertices;
    public float[] sphereNormals;
    public char[] sphereIndices;


    private int vertexIndex = 0;
    private int normalIndex = 0;
    private char indiceIndex = 0;
    private char triangleIndex = 0;


    public svrSphereMesh(int sphereStack, int sphereSlice, boolean lookToCenter) {
        createSphereMesh(sphereStack, sphereSlice, lookToCenter);
    }


    private void createSphereMesh(int sphereStack,
                                  int sphereSlice, boolean lookToCenter) {

        // assert sphereSlice>=4
        if (sphereSlice < 4) {
            throw new IllegalArgumentException(
                    "Slice should be equal or greater than 4.");
        }

        // assert sphereStack>=3
        if (sphereStack < 3) {
            throw new IllegalArgumentException(
                    "Stack should be equal or greater than 3.");
        }
        createSphere(sphereStack, sphereSlice, lookToCenter);

    }

    private void createSphere(int sphereStack, int sphereSlice,
                              boolean lookToCenter) {
        int vertexOfCap = 3 * sphereSlice;
        int vertexOfBody = 4 * sphereSlice * (sphereStack - 2);
        int vertexNumber = (2 * vertexOfCap) + vertexOfBody;
        int triangleNumber = (2 * vertexOfCap)
                + (6 * sphereSlice * (sphereStack - 2));

        sphereVertices = new float[5 * vertexNumber];
        sphereNormals = new float[3 * vertexNumber];
        sphereIndices = new char[triangleNumber];


        // bottom cap
        generateSphereCap(sphereStack, sphereSlice, false, lookToCenter);

        // body
        generateSphereBody(sphereStack, sphereSlice, lookToCenter);

        // top cap
        generateSphereCap(sphereStack, sphereSlice, true, lookToCenter);

    }

    private void generateSphereCap(int sphereStack, int sphereSlice, boolean top,
                                   boolean lookToCenter) {

        float currentStackIndex;
        float nextStackIndex;

        if (!top) {
            currentStackIndex = ((float) (sphereStack - 1) / sphereStack);
            nextStackIndex = 1.0f;

        } else {
            currentStackIndex = (1.0f / sphereStack);
            nextStackIndex = 0.0f;
        }

        float t0 = currentStackIndex;
        float t1 = nextStackIndex;
        double currentTheta = currentStackIndex * Math.PI;
        double nextTheta = nextStackIndex * Math.PI;
        double cosCurrentTheta = Math.cos(currentTheta);
        double sinCurrentTheta = Math.sin(currentTheta);
        double cosNextTheta = Math.cos(nextTheta);
        double sinNextTheta = Math.sin(nextTheta);

        for (int slice = 0; slice < sphereSlice; slice++) {
            float currentSliceIndex = ((float) (slice) / sphereSlice);
            float nextSliceIndex = ((float) (slice + 1) / sphereSlice);
            double currentPhi = currentSliceIndex * 2.0 * Math.PI;
            double nextPhi = nextSliceIndex * 2.0 * Math.PI;
            float s0, s1;
            if (!lookToCenter) {
                s0 = 1 - currentSliceIndex;
                s1 = 1 - nextSliceIndex;
            } else {
                s0 = currentSliceIndex;
                s1 = nextSliceIndex;
            }
            float s2 = (s0 + s1) / 2.0f;
            double cosCurrentPhi = Math.cos(currentPhi);
            double sinCurrentPhi = Math.sin(currentPhi);
            double cosNextPhi = Math.cos(nextPhi);
            double sinNextPhi = Math.sin(nextPhi);


            float z0 = (float) (sinCurrentTheta * cosCurrentPhi);
            float y0 = (float) cosCurrentTheta;
            float x0 = (float) (sinCurrentTheta * sinCurrentPhi);

            float z1 = (float) (sinCurrentTheta * cosNextPhi);
            float y1 = (float) cosCurrentTheta;
            float x1 = (float) (sinCurrentTheta * sinNextPhi);

            float z2 = (float) (sinNextTheta * cosCurrentPhi);
            float y2 = (float) cosNextTheta;
            float x2 = (float) (sinNextTheta * sinCurrentPhi);

            sphereVertices[vertexIndex + 0] = x0;
            sphereVertices[vertexIndex + 1] = y0;
            sphereVertices[vertexIndex + 2] = z0;

            sphereVertices[vertexIndex + 3] = s0;
            sphereVertices[vertexIndex + 4] = t0;

            sphereVertices[vertexIndex + 5] = x1;
            sphereVertices[vertexIndex + 6] = y1;
            sphereVertices[vertexIndex + 7] = z1;

            sphereVertices[vertexIndex + 8] = s1;
            sphereVertices[vertexIndex + 9] = t0;

            sphereVertices[vertexIndex + 10] = x2;
            sphereVertices[vertexIndex + 11] = y2;
            sphereVertices[vertexIndex + 12] = z2;

            sphereVertices[vertexIndex + 13] = s2;
            sphereVertices[vertexIndex + 14] = t1;

            if (lookToCenter) {
                sphereNormals[normalIndex + 0] = x0;
                sphereNormals[normalIndex + 1] = y0;
                sphereNormals[normalIndex + 2] = z0;

                sphereNormals[normalIndex + 3] = x1;
                sphereNormals[normalIndex + 4] = y1;
                sphereNormals[normalIndex + 5] = z1;

                sphereNormals[normalIndex + 6] = x2;
                sphereNormals[normalIndex + 7] = y2;
                sphereNormals[normalIndex + 8] = z2;
            } else {
                sphereNormals[normalIndex + 0] = -x0;
                sphereNormals[normalIndex + 1] = -y0;
                sphereNormals[normalIndex + 2] = -z0;

                sphereNormals[normalIndex + 3] = -x1;
                sphereNormals[normalIndex + 4] = -y1;
                sphereNormals[normalIndex + 5] = -z1;

                sphereNormals[normalIndex + 6] = -x2;
                sphereNormals[normalIndex + 7] = -y2;
                sphereNormals[normalIndex + 8] = -z2;
            }


            if ((!lookToCenter && top) || (lookToCenter && !top)) {
                sphereIndices[indiceIndex + 0] = (char) (triangleIndex + 1);
                sphereIndices[indiceIndex + 1] = (char) (triangleIndex + 0);
                sphereIndices[indiceIndex + 2] = (char) (triangleIndex + 2);
            } else {
                sphereIndices[indiceIndex + 0] = (char) (triangleIndex + 0);
                sphereIndices[indiceIndex + 1] = (char) (triangleIndex + 1);
                sphereIndices[indiceIndex + 2] = (char) (triangleIndex + 2);
            }

            vertexIndex += 15;
            normalIndex += 9;
            indiceIndex += 3;
            triangleIndex += 3;
        }

    }

    private void generateSphereBody(int sphereStack, int sphereSlice, boolean lookToCenter) {
        for (int stack = 1; stack < sphereStack - 1; stack++) {
            float currentStackIndex = ((float) (stack) / sphereStack);
            float nextStackIndex = ((float) (stack + 1) / sphereStack);

            float t0 = currentStackIndex;
            float t1 = nextStackIndex;

            double currentTheta = currentStackIndex * Math.PI;
            double nextTheta = nextStackIndex * Math.PI;
            double cosCurrentTheta = Math.cos(currentTheta);
            double sinCurrentTheta = Math.sin(currentTheta);
            double cosNextTheta = Math.cos(nextTheta);
            double sinNextTheta = Math.sin(nextTheta);

            for (int slice = 0; slice < sphereSlice; slice++) {
                float currentSliceIndex = ((float) (slice) / sphereSlice);
                float nextSliceIndex = ((float) (slice + 1) / sphereSlice);
                double currentPhi = currentSliceIndex * 2.0 * Math.PI;
                double nextPhi = nextSliceIndex * 2.0 * Math.PI;
                float s0, s1;
                if (!lookToCenter) {
                    s0 = 1.0f - currentSliceIndex;
                    s1 = 1.0f - nextSliceIndex;
                } else {
                    s0 = currentSliceIndex;
                    s1 = nextSliceIndex;
                }
                double cosCurrentPhi = Math.cos(currentPhi);
                double sinCurrentPhi = Math.sin(currentPhi);
                double cosNextPhi = Math.cos(nextPhi);
                double sinNextPhi = Math.sin(nextPhi);

                float z0 = (float) (sinCurrentTheta * cosCurrentPhi);
                float y0 = (float) cosCurrentTheta;
                float x0 = (float) (sinCurrentTheta * sinCurrentPhi);

                float z1 = (float) (sinCurrentTheta * cosNextPhi);
                float y1 = (float) cosCurrentTheta;
                float x1 = (float) (sinCurrentTheta * sinNextPhi);

                float z2 = (float) (sinNextTheta * cosCurrentPhi);
                float y2 = (float) cosNextTheta;
                float x2 = (float) (sinNextTheta * sinCurrentPhi);

                float z3 = (float) (sinNextTheta * cosNextPhi);
                float y3 = (float) cosNextTheta;
                float x3 = (float) (sinNextTheta * sinNextPhi);


                sphereVertices[vertexIndex + 0] = x0;
                sphereVertices[vertexIndex + 1] = y0;
                sphereVertices[vertexIndex + 2] = z0;

                sphereVertices[vertexIndex + 3] = s0;
                sphereVertices[vertexIndex + 4] = t0;

                sphereVertices[vertexIndex + 5] = x1;
                sphereVertices[vertexIndex + 6] = y1;
                sphereVertices[vertexIndex + 7] = z1;

                sphereVertices[vertexIndex + 8] = s1;
                sphereVertices[vertexIndex + 9] = t0;

                sphereVertices[vertexIndex + 10] = x2;
                sphereVertices[vertexIndex + 11] = y2;
                sphereVertices[vertexIndex + 12] = z2;

                sphereVertices[vertexIndex + 13] = s0;
                sphereVertices[vertexIndex + 14] = t1;

                sphereVertices[vertexIndex + 15] = x3;
                sphereVertices[vertexIndex + 16] = y3;
                sphereVertices[vertexIndex + 17] = z3;

                sphereVertices[vertexIndex + 18] = s1;
                sphereVertices[vertexIndex + 19] = t1;

                if (lookToCenter) {
                    sphereNormals[normalIndex + 0] = x0;
                    sphereNormals[normalIndex + 1] = y0;
                    sphereNormals[normalIndex + 2] = z0;

                    sphereNormals[normalIndex + 3] = x1;
                    sphereNormals[normalIndex + 4] = y1;
                    sphereNormals[normalIndex + 5] = z1;

                    sphereNormals[normalIndex + 6] = x2;
                    sphereNormals[normalIndex + 7] = y2;
                    sphereNormals[normalIndex + 8] = z2;

                    sphereNormals[normalIndex + 9] = x3;
                    sphereNormals[normalIndex + 10] = y3;
                    sphereNormals[normalIndex + 11] = z3;
                } else {
                    sphereNormals[normalIndex + 0] = -x0;
                    sphereNormals[normalIndex + 1] = -y0;
                    sphereNormals[normalIndex + 2] = -z0;

                    sphereNormals[normalIndex + 3] = -x1;
                    sphereNormals[normalIndex + 4] = -y1;
                    sphereNormals[normalIndex + 5] = -z1;

                    sphereNormals[normalIndex + 6] = -x2;
                    sphereNormals[normalIndex + 7] = -y2;
                    sphereNormals[normalIndex + 8] = -z2;

                    sphereNormals[normalIndex + 9] = -x3;
                    sphereNormals[normalIndex + 10] = -y3;
                    sphereNormals[normalIndex + 11] = -z3;
                }

                if (!lookToCenter) {
                    sphereIndices[indiceIndex + 0] = (char) (triangleIndex + 0);
                    sphereIndices[indiceIndex + 1] = (char) (triangleIndex + 1);
                    sphereIndices[indiceIndex + 2] = (char) (triangleIndex + 2);

                    sphereIndices[indiceIndex + 3] = (char) (triangleIndex + 2);
                    sphereIndices[indiceIndex + 4] = (char) (triangleIndex + 1);
                    sphereIndices[indiceIndex + 5] = (char) (triangleIndex + 3);
                } else {
                    sphereIndices[indiceIndex + 0] = (char) (triangleIndex + 0);
                    sphereIndices[indiceIndex + 1] = (char) (triangleIndex + 2);
                    sphereIndices[indiceIndex + 2] = (char) (triangleIndex + 1);

                    sphereIndices[indiceIndex + 3] = (char) (triangleIndex + 2);
                    sphereIndices[indiceIndex + 4] = (char) (triangleIndex + 3);
                    sphereIndices[indiceIndex + 5] = (char) (triangleIndex + 1);
                }

                vertexIndex += 20;
                normalIndex += 12;
                indiceIndex += 6;
                triangleIndex += 4;
            }
        }

    }

}
