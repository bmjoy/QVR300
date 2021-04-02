//-----------------------------------------------------------------------------
//  Copyright (c) 2017 Qualcomm Technologies, Inc.
//  All Rights Reserved. Qualcomm Technologies Proprietary and Confidential.
//-----------------------------------------------------------------------------

package com.qualcomm.svrapi.controllers;

import android.util.Log;

/**
 *
 * @param <T>
 */
class IndexedList<T> {
    Object[] list = null;
    int offset = 0;

    //-------------------------------------------------------------------------
    IndexedList(int size)
    {
        list = new Object[size];
        offset = 0;
    }

    //-------------------------------------------------------------------------
    int Size()
    {
        return list.length;
    }

    //-------------------------------------------------------------------------
    void Dump()
    {
        for(int i=0;i<list.length;i++)
        {
            Log.e("XXX", "ArrayList[" + i + "] = " + list[(i)]);
        }

    }
    //-------------------------------------------------------------------------
    int Add(T obj)
    {
        int index = FreeEntry();
        if(index >= 0 ) {
            Set(index, obj);
            offset = (offset + 1)%list.length;
        }

        //Dump();
        
        return index;
    }
    //-------------------------------------------------------------------------
    private int FreeEntry()
    {
        int index = -1;
        for(int i=0;i<list.length;i++)
        {
            int s = (offset +i)%list.length;
            if( list[s] == null )
            {
                index = s;
                break;
            }
        }
        return index;
    }

    //-------------------------------------------------------------------------
    @SuppressWarnings("unchecked")
    T Get(int index)
    {
        T result = null;
        if( (index >=0) && (index < list.length) )
        {
            result = (T)list[index];
        }

        return result;
    }

    //-------------------------------------------------------------------------
    void Set(int index, T item)
    {
        if( (index >=0) && (index < list.length) )
        {
            list[index] = item;
        }
    }

    //-------------------------------------------------------------------------
    T Remove(int index)
    {
        T t = null;
        if( (index >= 0) && (index < list.length) ) {
            t = Get(index);
            Set(index, null);
        }

        return t;
    }
}

