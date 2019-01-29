/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 INRIA
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
 */

#include "simulator-impl.h"
#include "log.h"

/**
 * \file
 * \ingroup simulator
 * Implementation of class ns3::SimulatorImpl.
 */

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED (SimulatorImpl);
  
TypeId 
SimulatorImpl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SimulatorImpl")
    .SetParent<Object> ()
    .SetGroupName ("Core")
  ;
  return tid;
}

//below all added by wsun
void
SimulatorImpl::CountPacket (void)
{
	return ; // just stub function for base class
}

void
SimulatorImpl::CountAckPacket (void)
{
	return ; // just stub function for base class
}


void
SimulatorImpl::CountLossPacket (void)
{
	return ; // just stub function for base class
}

int 
SimulatorImpl::GetCount (void)
{
	return 0; // just stub function for base class
}

int 
SimulatorImpl::GetTransSpeed (void)
{
	return 0; // just stub function for base class
}

void
SimulatorImpl::ChangeState (void)
{
	return ; // just stub function for base class
}

int 
SimulatorImpl::GetLossCount (void)
{
	return 0; // just stub function for base class
}

int64_t
SimulatorImpl::GetInputTest (int i)
{
	return 0; // just stub function for base class
}

int64_t
SimulatorImpl::GetSwitchTime (int i)
{
	return 0; // just stub function for base class
}

void
SimulatorImpl::PushCount1 (int i)
{
	return ; // just stub function for base class
}

void
SimulatorImpl::PushCount2 (int i)
{
	return ; // just stub function for base class
}

int
SimulatorImpl::GetAppSpeed (int i)
{
	return 0; // just stub function for base class
}

int
SimulatorImpl::GetInputState (int i)
{
	return 0; // just stub function for base class
}

} // namespace ns3
