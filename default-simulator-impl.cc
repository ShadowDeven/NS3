/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "simulator.h"
#include "default-simulator-impl.h"
#include "scheduler.h"
#include "event-impl.h"
#include "double.h"
#include "ptr.h"
#include "pointer.h"
#include "assert.h"
#include "log.h"

#include <cmath>

#include <fstream> //used for open input configuration file 
#include "global-value.h" //used for random distribution initializion
#include "rng-seed-manager.h" //used for random seed manager
/**
 * \file
 * \ingroup simulator
 * Implementation of class ns3::DefaultSimulatorImpl.
 */

namespace ns3 {

// Note:  Logging in this file is largely avoided due to the
// number of calls that are made to these functions and the possibility
// of causing recursions leading to stack overflow
NS_LOG_COMPONENT_DEFINE ("DefaultSimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED (DefaultSimulatorImpl);

TypeId
DefaultSimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::DefaultSimulatorImpl")
    .SetParent<SimulatorImpl> ()
    .SetGroupName ("Core")
    .AddConstructor<DefaultSimulatorImpl> ()
  ;
  return tid;
}

DefaultSimulatorImpl::DefaultSimulatorImpl ()
{
  NS_LOG_FUNCTION (this);
  m_stop = false;
  // uids are allocated from 4.
  // uid 0 is "invalid" events
  // uid 1 is "now" events
  // uid 2 is "destroy" events
  m_uid = 4;
  // before ::Run is entered, the m_currentUid will be zero
  m_currentUid = 0;
  m_currentTs = 0;
  m_currentContext = 0xffffffff;
  m_unscheduledEvents = 0;
  m_eventsWithContextEmpty = true;
  m_main = SystemThread::Self();

// Initlization for shift gamma random distribution, could be changed for different distributions in the future
  DoubleValue alpha, beta, shift;
  GlobalValue::GetValueByName("Gamma_A",alpha);
  GlobalValue::GetValueByName("Gamma_B",beta);
  GlobalValue::GetValueByName("Gamma_SFT",shift);
  Gamma_shift = shift.Get();
  x= CreateObject<GammaRandomVariable> ();
  x->SetAttribute ("Alpha", alpha);
  x->SetAttribute ("Beta", beta);
	
  packet_counter = 0;
  loss_counter = 0;

// Initlization for uniform random distribution
  DoubleValue loss;
  double min = 0.0;
  double max = 100.0;
  GlobalValue::GetValueByName("Loss_RATE",loss);
  Loss_rate = loss.Get();
  x2 = CreateObject<UniformRandomVariable> ();
  x2->SetAttribute ("Min", DoubleValue (min));
  x2->SetAttribute ("Max", DoubleValue (max));
  Speed = 0; // no record;
  std::ifstream inputFile("/tmp/input_config.txt");

  if (inputFile)
  {
  int counter;
// read the parameters in the configuraion file into a vector
//e.g., speed alpha beta shift loss_rate app_speed rng_run curr_time
  inputFile >> counter;
  for (int i = 0; i < counter ; i ++)
  {
  struct Test_Parems new_tmp;
  inputFile >> new_tmp.speed;
  inputFile >> new_tmp.sftgma.alpha;
  inputFile >> new_tmp.sftgma.beta;
  inputFile >> new_tmp.sftgma.shift;
  inputFile >> new_tmp.Loss_rate;
  inputFile >> new_tmp.app_speed;
  inputFile >> new_tmp.rng_run;
  inputFile >> new_tmp.cur_time; // used to switch input parameter vector for feedback2
  
  RngSeedManager::SetRun (new_tmp.rng_run);
  new_tmp.gamma_rng= CreateObject<GammaRandomVariable> ();
  new_tmp.gamma_rng->SetAttribute ("Alpha", DoubleValue(new_tmp.sftgma.alpha));
  new_tmp.gamma_rng->SetAttribute ("Beta", DoubleValue(new_tmp.sftgma.beta));
  double min = 0.0;
  double max = 100.0;
  new_tmp.uniform_rng = CreateObject<UniformRandomVariable> ();
  new_tmp.uniform_rng->SetAttribute ("Min", DoubleValue (min));
  new_tmp.uniform_rng->SetAttribute ("Max", DoubleValue (max));
  vec_test_parems.push_back(new_tmp);
  }
  inputFile.close();
  
  markov_state = 0;
  Gamma_shift =vec_test_parems[markov_state].sftgma.shift;
  App_Speed =vec_test_parems[markov_state].app_speed;
  Loss_rate = vec_test_parems[markov_state].Loss_rate;
  x =vec_test_parems[markov_state].gamma_rng;
  x2 =vec_test_parems[markov_state].uniform_rng;
  Speed = vec_test_parems[markov_state].speed;
  if(vec_test_parems.size() > 1) // more than one input parameter vector
  {
  	switch_time = vec_test_parems[0].cur_time;
  }
  else
  {
  	switch_time = 0;
  }
  }
}

DefaultSimulatorImpl::~DefaultSimulatorImpl ()
{
  NS_LOG_FUNCTION (this);
}

void
DefaultSimulatorImpl::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  ProcessEventsWithContext ();

  while (!m_events->IsEmpty ())
    {
      Scheduler::Event next = m_events->RemoveNext ();
      next.impl->Unref ();
    }
  m_events = 0;
  SimulatorImpl::DoDispose ();

}
void
DefaultSimulatorImpl::ChangeState(void) //add a state transition, we thought it was a "markov transition"
{	
  int size =vec_test_parems.size() ;
  if (size > 1 )
  {
    if(markov_state < size - 1) // to add a new state 
    {
    int prev_state = markov_state;
    markov_state ++ ; // to add one more state
    Gamma_shift =vec_test_parems[markov_state].sftgma.shift;
    Loss_rate = vec_test_parems[markov_state].Loss_rate;
    x =vec_test_parems[markov_state].gamma_rng;
    x2 =vec_test_parems[markov_state].uniform_rng;
    Speed = vec_test_parems[markov_state].speed;
    App_Speed = vec_test_parems[markov_state].app_speed;
    
    int64_t delay = vec_test_parems[markov_state].cur_time - vec_test_parems[prev_state].cur_time;
    if (delay > 0)
    Simulator::Schedule (NanoSeconds(delay), &Simulator::ChangeState);
    }
  }	
  return;
}

void
DefaultSimulatorImpl::Destroy ()
{
  NS_LOG_FUNCTION (this);
  while (!m_destroyEvents.empty ()) 
    {
      Ptr<EventImpl> ev = m_destroyEvents.front ().PeekEventImpl ();
      m_destroyEvents.pop_front ();
      NS_LOG_LOGIC ("handle destroy " << ev);
      if (!ev->IsCancelled ())
        {
          ev->Invoke ();
        }
    }
}

void
DefaultSimulatorImpl::SetScheduler (ObjectFactory schedulerFactory)
{
  NS_LOG_FUNCTION (this << schedulerFactory);
  Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

  if (m_events != 0)
    {
      while (!m_events->IsEmpty ())
        {
          Scheduler::Event next = m_events->RemoveNext ();
          scheduler->Insert (next);
        }
    }
  m_events = scheduler;
}

// System ID for non-distributed simulation is always zero
uint32_t 
DefaultSimulatorImpl::GetSystemId (void) const
{
  return 0;
}

void
DefaultSimulatorImpl::ProcessOneEvent (void)
{
  Scheduler::Event next = m_events->RemoveNext ();
  NS_ASSERT (next.key.m_ts >= m_currentTs);
  m_unscheduledEvents--;

  NS_LOG_LOGIC ("handle " << next.key.m_ts);
  m_currentTs = next.key.m_ts;
  m_currentContext = next.key.m_context;
  m_currentUid = next.key.m_uid;
  next.impl->Invoke ();
  next.impl->Unref ();

  ProcessEventsWithContext ();
}

bool 
DefaultSimulatorImpl::IsFinished (void) const
{
  return m_events->IsEmpty () || m_stop;
}

void
DefaultSimulatorImpl::ProcessEventsWithContext (void)
{
  if (m_eventsWithContextEmpty)
    {
      return;
    }

  // swap queues
  EventsWithContext eventsWithContext;
  {
    CriticalSection cs (m_eventsWithContextMutex);
    m_eventsWithContext.swap(eventsWithContext);
    m_eventsWithContextEmpty = true;
  }
  while (!eventsWithContext.empty ())
    {
       EventWithContext event = eventsWithContext.front ();
       eventsWithContext.pop_front ();
       Scheduler::Event ev;
       ev.impl = event.event;
       ev.key.m_ts = m_currentTs + event.timestamp;
       ev.key.m_context = event.context;
       ev.key.m_uid = m_uid;
       m_uid++;
       m_unscheduledEvents++;
       m_events->Insert (ev);
    }
}

void
DefaultSimulatorImpl::Run (void)
{
  NS_LOG_FUNCTION (this);
  // Set the current threadId as the main threadId
  m_main = SystemThread::Self();
  ProcessEventsWithContext ();
  m_stop = false;

  while (!m_events->IsEmpty () && !m_stop) 
    {
      ProcessOneEvent ();
    }

  // If the simulator stopped naturally by lack of events, make a
  // consistency test to check that we didn't lose any events along the way.
  NS_ASSERT (!m_events->IsEmpty () || m_unscheduledEvents == 0);
}

void 
DefaultSimulatorImpl::Stop (void)
{
  NS_LOG_FUNCTION (this);
  m_stop = true;
}

void 
DefaultSimulatorImpl::Stop (Time const &delay)
{
  NS_LOG_FUNCTION (this << delay.GetTimeStep ());
  Simulator::Schedule (delay, &Simulator::Stop);
}

//
// Schedule an event for a _relative_ time in the future.
//
EventId
DefaultSimulatorImpl::Schedule (Time const &delay, EventImpl *event)
{
  NS_LOG_FUNCTION (this << delay.GetTimeStep () << event);
  NS_ASSERT_MSG (SystemThread::Equals (m_main), "Simulator::Schedule Thread-unsafe invocation!");

  Time tAbsolute = delay + TimeStep (m_currentTs);

  NS_ASSERT (tAbsolute.IsPositive ());
  NS_ASSERT (tAbsolute >= TimeStep (m_currentTs));
  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep ();
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

void
DefaultSimulatorImpl::ScheduleWithContext (uint32_t context, Time const &delay, EventImpl *event)
{
  NS_LOG_FUNCTION (this << context << delay.GetTimeStep () << event);

  if (SystemThread::Equals (m_main))
    {
      Time tAbsolute = delay + TimeStep (m_currentTs);
      Scheduler::Event ev;
      ev.impl = event;
      ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep ();
      ev.key.m_context = context;
      ev.key.m_uid = m_uid;
      m_uid++;
      m_unscheduledEvents++;
      m_events->Insert (ev);
    }
  else
    {
      EventWithContext ev;
      ev.context = context;
      // Current time added in ProcessEventsWithContext()
      ev.timestamp = delay.GetTimeStep ();
      ev.event = event;
      {
        CriticalSection cs (m_eventsWithContextMutex);
        m_eventsWithContext.push_back(ev);
        m_eventsWithContextEmpty = false;
      }
    }
}

EventId
DefaultSimulatorImpl::ScheduleNow (EventImpl *event)
{
  NS_ASSERT_MSG (SystemThread::Equals (m_main), "Simulator::ScheduleNow Thread-unsafe invocation!");

  Scheduler::Event ev;
  ev.impl = event;
  ev.key.m_ts = m_currentTs;
  ev.key.m_context = GetContext ();
  ev.key.m_uid = m_uid;
  m_uid++;
  m_unscheduledEvents++;
  m_events->Insert (ev);
  return EventId (event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
}

EventId
DefaultSimulatorImpl::ScheduleDestroy (EventImpl *event)
{
  NS_ASSERT_MSG (SystemThread::Equals (m_main), "Simulator::ScheduleDestroy Thread-unsafe invocation!");

  EventId id (Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
  m_destroyEvents.push_back (id);
  m_uid++;
  return id;
}

Time
DefaultSimulatorImpl::Now (void) const
{
  // Do not add function logging here, to avoid stack overflow
  return TimeStep (m_currentTs);
}

Time 
DefaultSimulatorImpl::GetDelayLeft (const EventId &id) const
{
  if (IsExpired (id))
    {
      return TimeStep (0);
    }
  else
    {
      return TimeStep (id.GetTs () - m_currentTs);
    }
}

void
DefaultSimulatorImpl::Remove (const EventId &id)
{
  if (id.GetUid () == 2)
    {
      // destroy events.  
      for (DestroyEvents::iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              m_destroyEvents.erase (i);
              break;
            }
        }
      return;
    }
  if (IsExpired (id))
    {
      return;
    }
  Scheduler::Event event;
  event.impl = id.PeekEventImpl ();
  event.key.m_ts = id.GetTs ();
  event.key.m_context = id.GetContext ();
  event.key.m_uid = id.GetUid ();
  m_events->Remove (event);
  event.impl->Cancel ();
  // whenever we remove an event from the event list, we have to unref it.
  event.impl->Unref ();

  m_unscheduledEvents--;
}

void
DefaultSimulatorImpl::Cancel (const EventId &id)
{
  if (!IsExpired (id))
    {
      id.PeekEventImpl ()->Cancel ();
    }
}

bool
DefaultSimulatorImpl::IsExpired (const EventId &id) const
{
  if (id.GetUid () == 2)
    {
      if (id.PeekEventImpl () == 0 ||
          id.PeekEventImpl ()->IsCancelled ())
        {
          return true;
        }
      // destroy events.
      for (DestroyEvents::const_iterator i = m_destroyEvents.begin (); i != m_destroyEvents.end (); i++)
        {
          if (*i == id)
            {
              return false;
            }
        }
      return true;
    }
  if (id.PeekEventImpl () == 0 ||
      id.GetTs () < m_currentTs ||
      (id.GetTs () == m_currentTs &&
       id.GetUid () <= m_currentUid) ||
      id.PeekEventImpl ()->IsCancelled ()) 
    {
      return true;
    }
  else
    {
      return false;
    }
}

Time 
DefaultSimulatorImpl::GetMaximumSimulationTime (void) const
{
  /// \todo I am fairly certain other compilers use other non-standard
  /// post-fixes to indicate 64 bit constants.
  return TimeStep (0x7fffffffffffffffLL);
}

uint32_t
DefaultSimulatorImpl::GetContext (void) const
{
  return m_currentContext;
}

void
DefaultSimulatorImpl::CountPacket (void) 
{
  packet_counter++;
}

void
DefaultSimulatorImpl::CountLossPacket (void) 
{
  loss_counter++;
}

void
DefaultSimulatorImpl::CountAckPacket (void) 
{
  ack_counter++;
}

int
DefaultSimulatorImpl::GetCount (void) 
{
  return packet_counter;
}

int
DefaultSimulatorImpl::GetTransSpeed (void) 
{
  return Speed; // now used for data transmission speed, Mbps unit;
}

int 
DefaultSimulatorImpl::GetLossCount (void) 
{
  return loss_counter;
}

int64_t
DefaultSimulatorImpl::GetInputTest (int i)
{
  if (m_currentTs <= 1000000000) // less than 1 ms, used for tcp connection established 
    return 1000000 ; // 1ms
  
  int64_t value = -1; // -1 signal packet loss
  double loss_value = x2->GetValue ();//uniform random
  if (loss_value >= Loss_rate) //Not lost
  {
    //double gma_val = x->GetValue ();
    //value = (gma_val + Gamma_shift)*1000000; //convert from ms to ns
	value = 100*1000000;
  }
  return value;
}

void
DefaultSimulatorImpl::PushCount1 (int i)
{
  data_counts.push_back(i);// record data packet 
  return;
}

void
DefaultSimulatorImpl::PushCount2 (int i)
{
  ack_counts.push_back(i); // record ack packet
  return;
}

int
DefaultSimulatorImpl::GetAppSpeed (int i)
{
  return App_Speed;//now used for pass app_speed
}

int
DefaultSimulatorImpl::GetInputState (int i)
{
  return markov_state;//Transition State
}

int64_t
DefaultSimulatorImpl::GetSwitchTime (int i)
{
  return switch_time;
}
} // namespace ns3
