package main

import (
	"fmt"
	"github.com/gin-gonic/gin"
	"net/http"
	"sync"
	"time"
)

const (
	epoch             = int64(1622505600000) 
	workerIdBits      = 5
	datacenterIdBits  = 5
	sequenceBits      = 12

	maxWorkerId       = (1 << workerIdBits) - 1
	maxDatacenterId   = (1 << datacenterIdBits) - 1
	sequenceMask      = (1 << sequenceBits) - 1

	workerIdShift     = sequenceBits
	datacenterIdShift = sequenceBits + workerIdBits
	timestampLeftShift = sequenceBits + workerIdBits + datacenterIdBits
)

type SnowflakeIdGenerator struct {
	mutex         sync.Mutex
	lastTimestamp int64
	workerId      int64
	datacenterId  int64
	sequence      int64
}

func NewSnowflakeIdGenerator(workerId, datacenterId int64) *SnowflakeIdGenerator {
	if workerId > maxWorkerId || workerId < 0 {
		panic(fmt.Sprintf("Worker ID can't be greater than %d or less than 0", maxWorkerId))
	}
	if datacenterId > maxDatacenterId || datacenterId < 0 {
		panic(fmt.Sprintf("Datacenter ID can't be greater than %d or less than 0", maxDatacenterId))
	}
	return &SnowflakeIdGenerator{
		workerId:      workerId,
		datacenterId:  datacenterId,
		lastTimestamp: -1,
		sequence:      0,
	}
}

func (s *SnowflakeIdGenerator) NextId() int64 {
	s.mutex.Lock()
	defer s.mutex.Unlock()

	timestamp := timeGen()
	if timestamp < s.lastTimestamp {
		panic(fmt.Sprintf("Clock moved backwards. Refusing to generate ID for %d milliseconds", s.lastTimestamp-timestamp))
	}

	if timestamp == s.lastTimestamp {
		s.sequence = (s.sequence + 1) & sequenceMask
		if s.sequence == 0 {
			timestamp = tilNextMillis(s.lastTimestamp)
		}
	} else {
		s.sequence = 0
	}

	s.lastTimestamp = timestamp

	return ((timestamp - epoch) << timestampLeftShift) |
		(s.datacenterId << datacenterIdShift) |
		(s.workerId << workerIdShift) |
		s.sequence
}

func tilNextMillis(lastTimestamp int64) int64 {
	timestamp := timeGen()
	for timestamp <= lastTimestamp {
		timestamp = timeGen()
	}
	return timestamp
}

func timeGen() int64 {
	return time.Now().UnixNano() / int64(time.Millisecond)
}

func main() {
	r := gin.Default()
	generator := NewSnowflakeIdGenerator(1, 1)

	r.GET("/generate-id", func(c *gin.Context) {
		id := generator.NextId()
		c.JSON(http.StatusOK, gin.H{"id": id})
	})

	r.GET("/generate-ids", func(c *gin.Context) {
		count := 10 
		var ids []int64
		for i := 0; i < count; i++ {
			ids = append(ids, generator.NextId())
		}
		c.JSON(http.StatusOK, gin.H{"ids": ids})
	})

	r.Run(":8080")
}
