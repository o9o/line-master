/*
 *	Copyright (C) 2011 Ovidiu Mara
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef SPINLOCKEDQUEUE_H
#define SPINLOCKEDQUEUE_H

#include <pthread.h>
#include <QLinkedList>


#define PROFILE_SPINLOCKEDQUEUE 0

#if PROFILE_SPINLOCKEDQUEUE

#define SEC_TO_NSEC  1000000000
#define MSEC_TO_NSEC 1000000
#define USEC_TO_NSEC 1000

#define TS_FORMAT "%llu %3llus %3llum %3lluu %3llun"
#define TS_FORMAT_PARAM(X) (X)/SEC_TO_NSEC/1000, ((X)/SEC_TO_NSEC)%1000, ((X)/MSEC_TO_NSEC)%1000, ((X)/USEC_TO_NSEC)%1000, (X)%1000

quint64 get_current_time();

#endif

template<typename T>
class SpinlockedQueue {
public:
	SpinlockedQueue() {
		pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);
	}

	~SpinlockedQueue() {
		pthread_spin_destroy(&spinlock);
	}

	T dequeue()
	{
		T result = NULL;

		pthread_spin_lock(&spinlock);
		if (!items.isEmpty()) {
			result = items.takeFirst();
		}
		pthread_spin_unlock(&spinlock);

		return result;
	}

	QLinkedList<T> dequeueAll()
	{
		QLinkedList<T> result;

#if PROFILE_SPINLOCKEDQUEUE
		quint64 ts1 = get_current_time();
#endif

		pthread_spin_lock(&spinlock);
		if (!items.isEmpty()) {
			result = items;
			items.clear();
		}
		pthread_spin_unlock(&spinlock);

#if PROFILE_SPINLOCKEDQUEUE
		quint64 ts2 = get_current_time();
		if (ts2 - ts1 > 100 * USEC_TO_NSEC)
			printf("dequeue: ts delta = +"TS_FORMAT"\n", TS_FORMAT_PARAM(ts2 - ts1));
#endif

		return result;
	}

	T tryDequeue()
	{
		T result = NULL;

		if (pthread_spin_trylock(&spinlock) == 0) {
			if (!items.isEmpty()) {
				result = items.takeFirst();
			}
			pthread_spin_unlock(&spinlock);
		}

		return result;
	}

	void enqueue(T p)
	{
		pthread_spin_lock(&spinlock);
		items << p;
		pthread_spin_unlock(&spinlock);
	}

private:
	QLinkedList<T> items;
	pthread_spinlock_t spinlock;
};

#endif // SPINLOCKEDQUEUE_H
