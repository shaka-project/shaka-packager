// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import java.lang.Iterable;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

import javax.annotation.concurrent.NotThreadSafe;

/**
 * A container for a list of observers.
 * <p/>
 * This container can be modified during iteration without invalidating the iterator.
 * So, it safely handles the case of an observer removing itself or other observers from the list
 * while observers are being notified.
 * <p/>
 * The implementation (and the interface) is heavily influenced by the C++ ObserverList.
 * Notable differences:
 *   - The iterator implements NOTIFY_EXISTING_ONLY.
 *   - The FOR_EACH_OBSERVER closure is left to the clients to implement in terms of iterator().
 * <p/>
 * This class is not threadsafe. Observers MUST be added, removed and will be notified on the same
 * thread this is created.
 */
@NotThreadSafe
public class ObserverList<E> implements Iterable<E> {
    public final List<E> mObservers = new ArrayList<E>();
    private int mIterationDepth = 0;

    public ObserverList() {}

    /**
     * Add an observer to the list.
     * <p/>
     * An observer should not be added to the same list more than once. If an iteration is already
     * in progress, this observer will be not be visible during that iteration.
     */
    public void addObserver(E obs) {
        // Avoid adding null elements to the list as they may be removed on a compaction.
        if (obs == null || mObservers.contains(obs)) {
            assert false;
            return;
        }

        // Structurally modifying the underlying list here. This means we
        // cannot use the underlying list's iterator to iterate over the list.
        mObservers.add(obs);
    }

    /**
     * Remove an observer from the list if it is in the list.
     */
    public void removeObserver(E obs) {
        int index = mObservers.indexOf(obs);

        if (index == -1)
            return;

        if (mIterationDepth == 0) {
            // No one is iterating over the list.
            mObservers.remove(obs);
        } else {
            mObservers.set(index, null);
        }
    }

    public boolean hasObserver(E obs) {
        return mObservers.contains(obs);
    }

    public void clear() {
        if (mIterationDepth == 0) {
            mObservers.clear();
            return;
        }

        int size = mObservers.size();
        for (int i = 0; i < size; i++)
            mObservers.set(i, null);
    }

    @Override
    public Iterator<E> iterator() {
        return new ObserverListIterator();
    }

    /**
     * Compact the underlying list be removing null elements.
     * <p/>
     * Should only be called when mIterationDepth is zero.
     */
    private void compact() {
        assert mIterationDepth == 0;
        // Safe to use the underlying list's iterator, as we know that no-one else
        // is iterating over the list.
        Iterator<E> it = mObservers.iterator();
        while (it.hasNext()) {
            E el = it.next();
            if (el == null)
                it.remove();
        }
    }

    private void incrementIterationDepth() {
        mIterationDepth++;
    }

    private void decrementIterationDepthAndCompactIfNeeded() {
        mIterationDepth--;
        assert mIterationDepth >= 0;
        if (mIterationDepth == 0)
            compact();
    }

    private int getSize() {
        return mObservers.size();
    }

    private E getObserverAt(int index) {
        return mObservers.get(index);
    }

    private class ObserverListIterator implements Iterator<E> {
        private final int mListEndMarker;
        private int mIndex = 0;
        private boolean mIsExhausted = false;

        private ObserverListIterator() {
            ObserverList.this.incrementIterationDepth();
            mListEndMarker = ObserverList.this.getSize();
        }

        @Override
        public boolean hasNext() {
            int lookupIndex = mIndex;
            while (lookupIndex < mListEndMarker &&
                    ObserverList.this.getObserverAt(lookupIndex) == null)
                lookupIndex++;
            if (lookupIndex < mListEndMarker)
                return true;

            // We have reached the end of the list, allow for compaction.
            compactListIfNeeded();
            return false;
        }

        @Override
        public E next() {
            // Advance if the current element is null.
            while (mIndex < mListEndMarker && ObserverList.this.getObserverAt(mIndex) == null)
                mIndex++;
            if (mIndex < mListEndMarker)
                return ObserverList.this.getObserverAt(mIndex++);

            // We have reached the end of the list, allow for compaction.
            compactListIfNeeded();
            throw new NoSuchElementException();
        }

        @Override
        public void remove() {
            throw new UnsupportedOperationException();
        }

        private void compactListIfNeeded() {
            if (!mIsExhausted) {
                mIsExhausted = true;
                ObserverList.this.decrementIterationDepthAndCompactIfNeeded();
            }
        }
    }
}
