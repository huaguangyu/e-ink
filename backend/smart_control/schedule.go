package main

import (
	"fmt"
	"net/http"
	"sort"
	"strconv"
	"strings"
	"time"

	"github.com/gin-gonic/gin"
)

// calcSleepMin determines the sleep interval based on the schedule and current time.
func calcSleepMin(schedule *SleepSchedule, now time.Time) int {
	if schedule == nil || schedule.Mode == "fixed" {
		if schedule != nil && schedule.FixedMin > 0 {
			return schedule.FixedMin
		}
		return 5
	}

	if len(schedule.Slots) == 0 {
		if schedule.DefaultMin > 0 {
			return schedule.DefaultMin
		}
		return 5
	}

	nowMins := now.Hour()*60 + now.Minute()

	// Expand slots into absolute ranges on a [0, 2880) timeline.
	// Normal slot: [start, end)
	// Cross-midnight slot (end <= start): [start, end+1440)
	type slotRange struct {
		start, end, interval int
	}
	var ranges []slotRange
	for _, s := range schedule.Slots {
		sm := parseHHMM(s.Start)
		em := parseHHMM(s.End)
		if sm < 0 || em < 0 {
			continue
		}
		iv := s.IntervalMin
		if iv < 1 {
			iv = 1
		}
		if em <= sm {
			ranges = append(ranges, slotRange{sm, em + 1440, iv})
		} else {
			ranges = append(ranges, slotRange{sm, em, iv})
		}
	}
	sort.Slice(ranges, func(i, j int) bool { return ranges[i].start < ranges[j].start })

	// Try matching with nowMins (same-day).
	for _, r := range ranges {
		if nowMins >= r.start && nowMins < r.end {
			return r.interval
		}
	}

	// Try matching with nowMins+1440 (cross-midnight perspective).
	for _, r := range ranges {
		shifted := nowMins + 1440
		if shifted >= r.start && shifted < r.end {
			return r.interval
		}
	}

	// Not in any slot: find gap to next slot start.
	// Check same-day starts.
	nextStart := -1
	for _, r := range ranges {
		if r.start > nowMins {
			if nextStart == -1 || r.start < nextStart {
				nextStart = r.start
			}
		}
	}
	if nextStart > 0 {
		return nextStart - nowMins
	}

	// No more slots today: wrap to first slot tomorrow.
	if len(ranges) > 0 {
		return (1440 - nowMins) + ranges[0].start
	}

	if schedule.DefaultMin > 0 {
		return schedule.DefaultMin
	}
	return 5
}

func parseHHMM(s string) int {
	parts := strings.SplitN(s, ":", 2)
	if len(parts) != 2 {
		return -1
	}
	h, err := strconv.Atoi(parts[0])
	if err != nil || h < 0 || h > 23 {
		return -1
	}
	m, err := strconv.Atoi(parts[1])
	if err != nil || m < 0 || m > 59 {
		return -1
	}
	return h*60 + m
}

// validateSchedule checks the request for correctness.
func validateSchedule(req ScheduleRequest) error {
	switch req.Mode {
	case "fixed", "custom":
	default:
		return fmt.Errorf("mode must be 'fixed' or 'custom'")
	}

	if req.Mode == "fixed" {
		if req.FixedMin < 1 || req.FixedMin > 1440 {
			return fmt.Errorf("fixed_min must be 1-1440")
		}
		return nil
	}

	if req.DefaultMin < 1 || req.DefaultMin > 1440 {
		return fmt.Errorf("default_min must be 1-1440")
	}
	if len(req.Slots) > 24 {
		return fmt.Errorf("too many slots (max 24)")
	}

	type slotBound struct {
		start, end int
	}
	var bounds []slotBound
	for i, s := range req.Slots {
		sm := parseHHMM(s.Start)
		em := parseHHMM(s.End)
		if sm < 0 {
			return fmt.Errorf("slot %d: invalid start time '%s'", i+1, s.Start)
		}
		if em < 0 {
			return fmt.Errorf("slot %d: invalid end time '%s'", i+1, s.End)
		}
		if sm == em {
			return fmt.Errorf("slot %d: start and end cannot be the same", i+1)
		}
		if s.IntervalMin < 1 || s.IntervalMin > 1440 {
			return fmt.Errorf("slot %d: interval_min must be 1-1440", i+1)
		}

		// Expand cross-midnight for overlap detection.
		if em <= sm {
			em += 1440
		}
		bounds = append(bounds, slotBound{sm, em})
	}

	sort.Slice(bounds, func(i, j int) bool { return bounds[i].start < bounds[j].start })
	for i := 1; i < len(bounds); i++ {
		if bounds[i].start < bounds[i-1].end {
			return fmt.Errorf("slots overlap: one ends at minute %d but next starts at %d", bounds[i-1].end, bounds[i].start)
		}
	}

	return nil
}

func (s *Server) handleGetSchedule(c *gin.Context) {
	mac := normalizeMAC(c.Query("mac"))
	if mac == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "mac is required"})
		return
	}
	schedule, err := s.store.GetSleepSchedule(mac)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	if schedule == nil {
		c.JSON(http.StatusOK, gin.H{"schedule": nil})
		return
	}
	c.JSON(http.StatusOK, gin.H{"schedule": schedule})
}

func (s *Server) handlePutSchedule(c *gin.Context) {
	var req ScheduleRequest
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	req.MAC = normalizeMAC(req.MAC)
	if req.MAC == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "mac is required"})
		return
	}
	if err := validateSchedule(req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
		return
	}
	schedule, err := s.store.UpsertSleepSchedule(req)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	s.hub.Broadcast("schedule_changed", schedule)
	c.JSON(http.StatusOK, gin.H{"schedule": schedule})
}
