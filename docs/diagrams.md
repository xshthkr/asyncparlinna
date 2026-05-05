Sequential ParLinNa Application Flow Sequence Diagram

```
@startuml

caller->mpi_proc: ParLinNa(buffer)
activate mpi_proc

hnote over mpi_proc
Phase 1
Compute bound
endhnote

mpi_proc->mpi_proc: ParLogNa intra-node\naggregation

hnote over mpi_proc, NIC
Phase 2
Network bound
endhnote

mpi_proc->NIC: MPI_Isend / MPI_Irecv
activate NIC

NIC-->mpi_proc: MPI_Waitall
deactivate NIC

mpi_proc-->caller: return
deactivate mpi_proc

@enduml
```

Async ParLinNa Application Flow and Asynchrony Sequence Diagram

```
@startuml

caller->mpi_proc: AsyncParLinNa(buffer)
activate mpi_proc

mpi_proc->mpi_proc: ParLogNa intra-node\naggregation


mpi_proc->dbuffer: push payload (state: READY)

mpi_proc-->caller: return asyc

deactivate mpi_proc
activate caller

dbuffer->pthread: state == READY
activate pthread

pthread->NIC: MPI_Isend / MPI_Irecv
NIC-->pthread: MPI_Waitall

pthread->dbuffer: mark DONE
deactivate pthread

caller->mpi_proc: servlet_wait()
activate mpi_proc

mpi_proc->dbuffer: await DONE

mpi_proc-->caller: return
deactivate mpi_proc

@enduml
```

Sequential ParLinNa Execution Timeline Gantt Chart

```
@startgantt
--first call--
[Phase 1 (buffer 1)] starts D+0
[Phase 1 (buffer 1)] ends D+9
[Phase 2 (buffer 1)] starts D+10
[Phase 2 (buffer 1)] ends D+19
--second call--
[Phase 1 (buffer 2)] starts D+20
[Phase 1 (buffer 2)] ends D+29
[Phase 2 (buffer 2)] starts D+30
[Phase 2 (buffer 2)] ends D+39

Separator just at [Phase 1 (buffer 1)]'s start
Separator just at [Phase 2 (buffer 1)]'s end

@endgantt
```

Async ParLinNa Execution Overlap Timeline Gantt Chart

```
@startgantt
--first call--
[Phase 1 (buffer 1)] starts D+0
[Phase 1 (buffer 1)] ends D+9
[Phase 2 (buffer 1)] starts D+10
[Phase 2 (buffer 1)] ends D+19
--second call --
[Phase 1 (buffer 2)] starts D+10
[Phase 1 (buffer 2)] ends D+19
[Phase 2 (buffer 2)] starts D+20
[Phase 2 (buffer 2)] ends D+29

Separator just at [Phase 1 (buffer 1)]'s start
Separator just at [Phase 2 (buffer 1)]'s end
Separator just at [Phase 1 (buffer 2)]'s start
Separator just at [Phase 2 (buffer 2)]'s end

@endgantt
```

Async ParLinNa Lock Free Synchronization Pipeline State Diagram

```
@startuml 
hide empty description
[*] --> IDLE

IDLE --> READY : Main Thread\n(memory_order_release)
note left of IDLE
Main Thread claims slot,
writes Phase 1 payload.
end note

READY --> DONE : Servlet Thread\n(memory_order_release)
note right of READY
Servlet detects READY,
executes Phase 2 network scatter.
end note

DONE --> IDLE : Main Thread\n(memory_order_release)
note left of DONE
Main Thread reclaims slot
for the next API invocation.
end note
@enduml
```

Async ParLinNa Hardware Topology and Thread Pinning Graph

```
@startuml
hide empty description

' Define Styling
skinparam state {
  BackgroundColor<<ServletThread>> #f9f
  BackgroundColor<<Hardware>> White
  BorderColor<<NIC>> #bbf
}

state "HPC Node" as HPC_NODE <<Root>> {
  state "NUMA Node 0" as NUMA_0 {
    state CPU0 <<MainThread>> {
      state "CPU Core 0\n(Main Thread)" as CPU0_label
    }
    state CPU1 <<MainThread>> {
      state "CPU Core 1\n(Main Thread)" as CPU1_label
    }
    state Mem0 <<Hardware>> {
      state "System RAM" as Mem0_label
    }
    
    CPU0_label -right-> Mem0_label : memory access
    CPU1_label -left-> Mem0_label : memory access
  }

  state "NUMA Node 1" as NUMA_1 {
    state CPU2 <<MainThread>> {
      state "CPU Core 2\n(Main Thread)" as CPU2_label
    }
    state CPU3 <<ServletThread>> {
      state "CPU Core 3\n(Servlet Thread)" as CPU3_label
    }
    state Mem1 <<Hardware>> {
      state "System RAM" as Mem1_label
    }
    state NIC <<NIC>> {
      state "Network Interface Card\n(10 Gigabit Ethernet)" as NIC_label
    }
    
    CPU2_label -up-> Mem1_label : memory access
    CPU3_label -left-> Mem1_label : memory access
    CPU3_label --> NIC_label : Direct PCIe\n(Low Latency)
  }
  
  NUMA_0 -down-> NUMA_1 : Interconnect\n(High Latency)

}
@enduml
```
